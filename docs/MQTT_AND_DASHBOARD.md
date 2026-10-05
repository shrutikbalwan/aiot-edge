# MQTT and dashboard integration

The firmware uses ESP-IDF `esp-mqtt`. Local simulation may connect to Mosquitto over plaintext on a trusted development network. A sample broker command is:

```sh
mosquitto -v -p 1883
```

Set the broker URI and device ID in `idf.py menuconfig`, enable MQTT, build, and subscribe with:

```sh
mosquitto_sub -h 192.168.1.2 -t 'aiot/v1/devices/+/telemetry' -q 1 -v
```

Production uses `mqtts://`, the ESP certificate bundle, and hostname verification. Credentials belong in runtime provisioning or an untracked local configuration, never browser JavaScript or tracked source.

The static dashboard deliberately does not implement broker authentication. A backend should subscribe to MQTT, authorize the browser, and send either a telemetry object or `{ "topic": "...", "payload": { ... } }` over `wss://`. The payload schema matches firmware JSON (`schema`, `timestamp_ms`, optional health estimates, temperature/acceleration, and `simulated`). The browser rejects simulated payloads on a live connection.
