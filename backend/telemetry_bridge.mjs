import {createServer} from "node:http";
import {timingSafeEqual} from "node:crypto";
import {pathToFileURL} from "node:url";
import mqtt from "mqtt";
import {WebSocket, WebSocketServer} from "ws";
import {validateTelemetry} from "../www/app.js";

const MAX_MQTT_PAYLOAD = 4096;
const MAX_WS_BUFFERED_BYTES = 64 * 1024;

function secureEqual(left, right) {
  const a = Buffer.from(left ?? "", "utf8");
  const b = Buffer.from(right ?? "", "utf8");
  return a.length === b.length && timingSafeEqual(a, b);
}

export function authorizeProtocols(header, expectedToken) {
  if (typeof header !== "string" || expectedToken.length < 32) return false;
  const protocols = header.split(",").map((value) => value.trim());
  return protocols[0] === "aiot-v1" && protocols.length === 2 &&
    secureEqual(protocols[1], expectedToken);
}

export function validateBridgeConfig(environment) {
  const brokerUrl = environment.AIOT_MQTT_URL ?? "";
  const token = environment.AIOT_WS_TOKEN ?? "";
  if (!/^mqtts:\/\//.test(brokerUrl) && environment.AIOT_ALLOW_PLAINTEXT_MQTT !== "1") {
    throw new Error("AIOT_MQTT_URL must use mqtts:// unless plaintext is explicitly enabled");
  }
  if (!/^[A-Za-z0-9._~-]{32,128}$/.test(token)) {
    throw new Error("AIOT_WS_TOKEN must be a 32-128 character URL-safe token");
  }
  const port = Number(environment.AIOT_WS_PORT ?? "8080");
  if (!Number.isInteger(port) || port < 1 || port > 65535) throw new Error("invalid AIOT_WS_PORT");
  return {
    brokerUrl,
    token,
    host: environment.AIOT_WS_HOST ?? "127.0.0.1",
    port,
    topic: `${environment.AIOT_TOPIC_PREFIX ?? "aiot/v1"}/devices/${environment.AIOT_DEVICE_ID ?? "+"}/telemetry`,
    username: environment.AIOT_MQTT_USERNAME,
    password: environment.AIOT_MQTT_PASSWORD,
  };
}

export function decodeTelemetry(buffer) {
  if (!Buffer.isBuffer(buffer) || buffer.length === 0 || buffer.length > MAX_MQTT_PAYLOAD) return null;
  try {
    return validateTelemetry(JSON.parse(buffer.toString("utf8")));
  } catch {
    return null;
  }
}

export function startBridge(environment = process.env) {
  const config = validateBridgeConfig(environment);
  const httpServer = createServer((request, response) => {
    response.writeHead(request.url === "/health" ? 200 : 404,
      {"content-type": "application/json", "cache-control": "no-store"});
    response.end(JSON.stringify({status: request.url === "/health" ? "ok" : "not_found"}));
  });
  const websocketServer = new WebSocketServer({
    noServer: true,
    handleProtocols: (protocols) => protocols.has("aiot-v1") ? "aiot-v1" : false,
  });
  httpServer.on("upgrade", (request, socket, head) => {
    if (request.url !== "/telemetry" ||
        !authorizeProtocols(request.headers["sec-websocket-protocol"], config.token)) {
      socket.write("HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n");
      socket.destroy();
      return;
    }
    websocketServer.handleUpgrade(request, socket, head, (client) => {
      websocketServer.emit("connection", client, request);
    });
  });

  const mqttClient = mqtt.connect(config.brokerUrl, {
    username: config.username,
    password: config.password,
    rejectUnauthorized: true,
    reconnectPeriod: 1000,
    connectTimeout: 15000,
  });
  mqttClient.on("connect", () => mqttClient.subscribe(config.topic, {qos: 1}, (error) => {
    if (error) console.error("MQTT subscription failed", error.message);
  }));
  mqttClient.on("message", (topic, payload) => {
    const sample = decodeTelemetry(payload);
    if (!sample) {
      console.warn("Rejected malformed telemetry", topic);
      return;
    }
    const message = JSON.stringify({topic, payload: sample});
    for (const client of websocketServer.clients) {
      if (client.readyState === WebSocket.OPEN && client.bufferedAmount < MAX_WS_BUFFERED_BYTES) {
        client.send(message);
      }
    }
  });
  mqttClient.on("error", (error) => console.error("MQTT connection error", error.message));
  httpServer.listen(config.port, config.host,
    () => console.log(`Telemetry bridge listening on ws://${config.host}:${config.port}/telemetry`));
  return {httpServer, mqttClient, websocketServer};
}

if (import.meta.url === pathToFileURL(process.argv[1] ?? "").href) startBridge();
