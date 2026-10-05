# MQTT and dashboard integration

The firmware uses ESP-IDF `esp-mqtt`. Local simulation may connect to Mosquitto over plaintext on a trusted development network. A sample broker command is:

```sh
mosquitto -v -p 1883
```

Set the broker URI and device ID in `idf.py menuconfig`, enable MQTT, build, and subscribe with:

```sh
mosquitto_sub -h 192.168.1.2 -t 'aiot/v1/devices/+/telemetry' -q 1 -v
```

Production uses `mqtts://`, the ESP certificate bundle, and hostname verification. The firmware loads its MQTT password from NVS. Command subscription is enabled only when TLS, a username, a runtime password, and command support are all present. The broker must apply per-device ACLs; transport authentication does not by itself decide which publisher may command a device.

The static dashboard deliberately does not contain broker credentials. The included `backend/telemetry_bridge.mjs` subscribes to MQTT and relays validated telemetry to authorized WebSocket clients. Configure it through process environment variables:

```sh
AIOT_MQTT_URL=mqtts://broker.example:8883 \
AIOT_MQTT_USERNAME=dashboard-reader \
AIOT_WS_TOKEN='replace-with-32-or-more-url-safe-characters' \
npm run bridge
```

Set `AIOT_MQTT_PASSWORD` through the process supervisor or secret store rather than a checked-in command or file. `AIOT_WS_TOKEN` accepts only 32-128 URL-safe characters and is compared in constant time. The relay binds to `127.0.0.1:8080` by default and serves WebSocket upgrades at `/telemetry`; expose it remotely only through a TLS reverse proxy. Plaintext broker transport requires the explicit `AIOT_ALLOW_PLAINTEXT_MQTT=1` development override. The payload schema matches firmware JSON (`schema`, `timestamp_ms`, optional health estimates, temperature/acceleration, and `simulated`). The relay caps messages at 4096 bytes and drops malformed input; the browser rejects simulated payloads on a live connection.
