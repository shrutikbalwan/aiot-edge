import test from "node:test";
import assert from "node:assert/strict";
import {authorizeProtocols, decodeTelemetry, validateBridgeConfig} from "../backend/telemetry_bridge.mjs";

const token = "0123456789abcdef0123456789abcdef";

test("requires TLS and a strong browser relay token", () => {
  assert.throws(() => validateBridgeConfig({AIOT_MQTT_URL: "mqtt://broker", AIOT_WS_TOKEN: token}));
  assert.throws(() => validateBridgeConfig({AIOT_MQTT_URL: "mqtts://broker", AIOT_WS_TOKEN: "short"}));
  assert.throws(() => validateBridgeConfig({AIOT_MQTT_URL: "mqtts://broker", AIOT_WS_TOKEN: `${"a".repeat(31)} `}));
  const config = validateBridgeConfig({AIOT_MQTT_URL: "mqtts://broker", AIOT_WS_TOKEN: token});
  assert.equal(config.topic, "aiot/v1/devices/+/telemetry");
});

test("authorizes the websocket subprotocol without exposing broker credentials", () => {
  assert.equal(authorizeProtocols(`aiot-v1, ${token}`, token), true);
  assert.equal(authorizeProtocols("aiot-v1, wrong", token), false);
});

test("bounds and validates relayed telemetry", () => {
  const valid = Buffer.from(JSON.stringify({schema: 1, timestamp_ms: 4, simulated: false}));
  assert.equal(decodeTelemetry(valid)?.timestamp_ms, 4);
  assert.equal(decodeTelemetry(Buffer.from("not-json")), null);
  assert.equal(decodeTelemetry(Buffer.alloc(5000)), null);
});
