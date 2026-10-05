export const HISTORY_LIMIT = 100;
export const STALE_AFTER_MS = 15000;

export function validateTelemetry(value) {
  if (!value || typeof value !== "object" || value.schema !== 1) return null;
  const number = (name, min, max, required = false) => {
    const current = value[name];
    if (current === undefined && !required) return undefined;
    if (typeof current !== "number" || !Number.isFinite(current) || current < min || current > max) {
      throw new TypeError(`invalid ${name}`);
    }
    return current;
  };
  try {
    return {
      schema: 1,
      timestamp_ms: number("timestamp_ms", 0, Number.MAX_SAFE_INTEGER, true),
      heart_rate_bpm: number("heart_rate_bpm", 20, 250),
      spo2_permille: number("spo2_permille", 0, 1000),
      temperature_milli_c: number("temperature_milli_c", -50000, 100000),
      accel_x_mg: number("accel_x_mg", -32000, 32000),
      accel_y_mg: number("accel_y_mg", -32000, 32000),
      accel_z_mg: number("accel_z_mg", -32000, 32000),
      wake_word: typeof value.wake_word === "boolean" ? value.wake_word : false,
      simulated: value.simulated === true,
    };
  } catch {
    return null;
  }
}

export function boundedHistory(history, item, limit = HISTORY_LIMIT) {
  return [...history, item].slice(-limit);
}

export function csvForHistory(history) {
  const rows = ["timestamp_ms,mode,heart_rate_bpm,spo2_percent,temperature_c,accel_x_mg,accel_y_mg,accel_z_mg"];
  for (const item of history) {
    rows.push([item.timestamp_ms, item.simulated ? "simulation" : "live",
      item.heart_rate_bpm ?? "", item.spo2_permille === undefined ? "" : item.spo2_permille / 10,
      item.temperature_milli_c === undefined ? "" : item.temperature_milli_c / 1000,
      item.accel_x_mg ?? "", item.accel_y_mg ?? "", item.accel_z_mg ?? ""].join(","));
  }
  return `${rows.join("\n")}\n`;
}

if (typeof document !== "undefined") {
  const elements = Object.fromEntries(["mode", "endpoint", "connect", "disconnect", "simulation", "export",
    "connection-status", "heart-rate", "spo2", "temperature", "acceleration", "wake", "age", "history"]
    .map((id) => [id, document.getElementById(id)]));
  let socket = null;
  let simulationTimer = null;
  let reconnectTimer = null;
  let reconnectAttempt = 0;
  let lastReceivedAt = 0;
  let history = [];

  try {
    const stored = JSON.parse(localStorage.getItem("aiotHistory") || "[]");
    if (Array.isArray(stored)) history = stored.map(validateTelemetry).filter(Boolean).slice(-HISTORY_LIMIT);
    elements.endpoint.value = localStorage.getItem("aiotEndpoint") || "";
  } catch { history = []; }

  const setState = (label, className, detail) => {
    elements.mode.textContent = label;
    elements.mode.className = `badge ${className}`;
    elements["connection-status"].textContent = detail;
  };

  const render = (sample, mode) => {
    const display = (id, value, digits = 0) => {
      elements[id].textContent = value === undefined ? "—" : value.toFixed(digits);
    };
    display("heart-rate", sample.heart_rate_bpm);
    display("spo2", sample.spo2_permille === undefined ? undefined : sample.spo2_permille / 10, 1);
    display("temperature", sample.temperature_milli_c === undefined ? undefined : sample.temperature_milli_c / 1000, 2);
    elements.acceleration.textContent = sample.accel_x_mg === undefined ? "—" :
      `${sample.accel_x_mg}/${sample.accel_y_mg}/${sample.accel_z_mg}`;
    elements.wake.textContent = sample.wake_word ? "Detected" : "None";
    lastReceivedAt = Date.now();
    history = boundedHistory(history, {...sample, simulated: mode === "simulation"});
    localStorage.setItem("aiotHistory", JSON.stringify(history));
    elements.history.replaceChildren(...history.slice(-12).reverse().map((item) => {
      const row = document.createElement("tr");
      const values = [new Date(item.timestamp_ms).toLocaleTimeString(), item.simulated ? "SIM" : "LIVE",
        item.heart_rate_bpm ?? "—", item.spo2_permille === undefined ? "—" : (item.spo2_permille / 10).toFixed(1),
        item.temperature_milli_c === undefined ? "—" : (item.temperature_milli_c / 1000).toFixed(2),
        item.accel_x_mg === undefined ? "—" : `${item.accel_x_mg}/${item.accel_y_mg}/${item.accel_z_mg}`];
      for (const value of values) { const cell = document.createElement("td"); cell.textContent = String(value); row.append(cell); }
      return row;
    }));
  };

  const stopSimulation = () => {
    if (simulationTimer !== null) clearInterval(simulationTimer);
    simulationTimer = null;
    elements.simulation.textContent = "Start deterministic simulation";
  };

  const connect = () => {
    stopSimulation();
    const endpoint = elements.endpoint.value.trim();
    if (!/^wss?:\/\//.test(endpoint)) { setState("ERROR", "error", "Enter a ws:// or wss:// backend URL"); return; }
    localStorage.setItem("aiotEndpoint", endpoint);
    setState("CONNECTING", "offline", "Connecting to backend…");
    socket = new WebSocket(endpoint);
    elements.connect.disabled = true;
    elements.disconnect.disabled = false;
    socket.addEventListener("open", () => { reconnectAttempt = 0; setState("LIVE", "live", "Connected; waiting for telemetry"); });
    socket.addEventListener("message", (event) => {
      try {
        const envelope = JSON.parse(event.data);
        const sample = validateTelemetry(envelope.payload ?? envelope);
        if (!sample || sample.simulated) throw new TypeError("invalid live payload");
        render(sample, "live"); setState("LIVE", "live", "Live telemetry received");
      } catch { setState("MALFORMED", "error", "Rejected malformed telemetry"); }
    });
    socket.addEventListener("close", () => {
      socket = null; elements.connect.disabled = false; elements.disconnect.disabled = true;
      setState("OFFLINE", "offline", "Disconnected");
      if (elements.endpoint.value.trim()) {
        const delay = Math.min(30000, 1000 * (2 ** reconnectAttempt++));
        reconnectTimer = setTimeout(connect, delay);
      }
    });
    socket.addEventListener("error", () => setState("ERROR", "error", "WebSocket error"));
  };

  elements.connect.addEventListener("click", connect);
  elements.disconnect.addEventListener("click", () => {
    clearTimeout(reconnectTimer); reconnectAttempt = 0; elements.endpoint.value = "";
    if (socket) socket.close(); else setState("OFFLINE", "offline", "Disconnected");
  });
  elements.simulation.addEventListener("click", () => {
    if (simulationTimer !== null) { stopSimulation(); setState("OFFLINE", "offline", "Simulation stopped"); return; }
    clearTimeout(reconnectTimer); if (socket) socket.close();
    let step = 0;
    const emit = () => {
      const sample = validateTelemetry({schema: 1, timestamp_ms: Date.now(), simulated: true,
        heart_rate_bpm: 72 + (step % 5), spo2_permille: 980 - (step % 3),
        temperature_milli_c: 36500 + (step % 4) * 10,
        accel_x_mg: (step % 7) - 3, accel_y_mg: (step % 5) - 2, accel_z_mg: 1000});
      step += 1; render(sample, "simulation"); setState("SIMULATION", "simulation", "Deterministic demo data");
    };
    emit(); simulationTimer = setInterval(emit, 1000); elements.simulation.textContent = "Stop simulation";
  });
  elements.export.addEventListener("click", () => {
    const url = URL.createObjectURL(new Blob([csvForHistory(history)], {type: "text/csv"}));
    const link = document.createElement("a"); link.href = url; link.download = "aiot-edge-history.csv"; link.click(); URL.revokeObjectURL(url);
  });
  setInterval(() => {
    if (!lastReceivedAt) return;
    const age = (Date.now() - lastReceivedAt) / 1000; elements.age.textContent = age.toFixed(0);
    if (simulationTimer === null && socket?.readyState === WebSocket.OPEN && age * 1000 > STALE_AFTER_MS) {
      setState("STALE", "stale", "Connected, but telemetry is stale");
    }
  }, 1000);
}
