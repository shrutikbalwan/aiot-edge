# AIoT-Edge

AIoT-Edge is an ESP32-S3 ESP-IDF project for deterministic sensor simulation, educational health-signal estimates, BLE telemetry, provider-neutral MQTT, verified HTTPS OTA, a TinyML training/export workflow, and a browser dashboard.

Health outputs are engineering demonstrations, not medical devices or diagnoses. No repository result establishes clinical accuracy.

## Current status

| Area | Status | Evidence or boundary |
|---|---|---|
| Host signal processing | Verified | Native C test covers nominal, short, null, and flat windows. |
| Python/ML smoke pipeline | Verified | Pytest converts a full-int8 model, executes TFLite inference, and compiles the generated complete C array. |
| Dashboard parser/history | Verified | Node syntax check and three unit tests. |
| ESP32-S3 project structure | Verified | Clean default build completed with the official `espressif/idf:v5.1.6` environment; optional NimBLE/MQTT/OTA configuration also compiles. |
| MAX30102/MAX30205/BMI160 drivers | Implemented, hardware validation pending | Correct 7-bit addressing and bounded decoding; physical buses are untested. |
| NimBLE peripheral, MQTT, HTTPS OTA | Implemented, hardware validation pending | Optional and disabled by default; interoperability and provisioning require hardware/network testing. |
| CPU model-runtime adapter | Experimental | Interface exists; a production TFLite Micro/ESP-NN backend is not bundled. |
| Real wake-word dataset and device benchmark | Planned | Synthetic accuracy is only a pipeline smoke result. |

No custom ASIC, accelerator RTL, synthesis, timing closure, PUF implementation, or side-channel evaluation exists here.

## Architecture

```text
Sensors -> sensor queue -> health processing -> MQTT/BLE telemetry
Audio   -> feature extraction -> model runtime -> wake event
Commands -> MQTT/BLE bounded validation -> logged candidate (action dispatch disabled)
```

Resources are created before tasks. The sensor task owns sensor reads, the queue transfers initialized samples, and the processing task owns its complete PPG window. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Hardware

- ESP32-S3 only
- 4 MB or larger flash (the default partition table provides factory plus two 1 MB OTA slots)
- MAX30102 at 7-bit address `0x57`
- MAX30205 at configured base address `0x48`
- BMI160 at `0x68` or `0x69`
- Default I2C pins: SDA GPIO 8, SCL GPIO 9 (change with menuconfig)

## Prerequisites

- ESP-IDF 5.1 or newer with its environment exported
- Python 3.13 and packages pinned in `requirements-ml.txt` for the ML workflow
- Node.js 22 or newer for dashboard checks
- GCC for host C and generated-model syntax tests

## Build

The default build contains no credentials and uses deterministic simulation. BLE, MQTT, and OTA are disabled.

```sh
idf.py set-target esp32s3
idf.py build
```

For a clean rebuild:

```sh
idf.py fullclean
idf.py build
```

Use `idf.py menuconfig` under **AIoT-Edge** to disable simulation, choose the BMI160 address, or enable optional transports. Enabling NimBLE also requires the ESP-IDF NimBLE host option. Copy values from `config/examples/sdkconfig.defaults.local.example`; do not commit a generated `sdkconfig` containing secrets.

## Simulation and hardware setup

Simulation is deterministic by default and is explicitly logged. It generates a repeatable pulse and stationary acceleration. It does not prove sensor correctness.

For hardware, disable `CONFIG_AIOT_SIMULATION`, connect 3.3 V/GND and I2C with appropriate pull-ups, verify addresses with a bus analyzer or scanner, and validate output against trusted references. MAX30205 lacks a chip-ID register, so initialization checks bus communication/configuration; MAX30102 and BMI160 IDs are checked.

## MQTT

Topics are versioned:

- `aiot/v1/devices/{device_id}/telemetry` — QoS 1, not retained
- `aiot/v1/devices/{device_id}/events` — QoS 1, not retained (reserved)
- `aiot/v1/devices/{device_id}/commands` — QoS 1 subscription; publishers should not retain commands
- `aiot/v1/devices/{device_id}/ota/status` — QoS 1 (reserved)

For local Mosquitto development, simulation builds may use `mqtt://<LAN-IP>:1883`. Production configuration must use `mqtts://`; the code attaches the ESP certificate bundle and does not bypass hostname checks. Usernames can come from Kconfig for development, but passwords/private keys must be provisioned outside tracked files. The dashboard consumes a backend WebSocket JSON stream rather than embedding broker credentials; see [docs/MQTT_AND_DASHBOARD.md](docs/MQTT_AND_DASHBOARD.md).

## BLE and OTA

NimBLE exposes explicit, versioned binary telemetry and JSON command candidates. Notifications occur only while connected and subscribed, and payload size is checked against the negotiated MTU. Firmware bytes are not transferred over BLE. Because product authentication and authorization are not provisioned, received commands are validated and logged but do not execute device actions. See [docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md).

OTA accepts HTTPS URLs only, uses the certificate bundle, inspects the downloaded image description, rejects a version mismatch/downgrade by default, relies on ESP-IDF image verification, and selects the update partition only after `esp_https_ota_finish`. Secure Boot v2, flash encryption, eFuse secure version, and rollback behavior require physical provisioning and validation.

## ML workflow

Install the pinned environment, then run an explicitly synthetic smoke export:

```sh
python -m pip install -r requirements-ml.txt
python -m ml.pipeline --synthetic-demo --epochs 1 --embed-c
```

For real data, place one finite 40-element float32 `.npy` feature vector per file under `<dataset>/<label>/`, with at least ten samples per class:

```sh
python -m ml.pipeline --dataset path/to/features --epochs 20 --embed-c
```

Outputs are `wake_word.tflite`, `metadata.json`, `labels.txt`, `evaluation.json`, and optionally `wake_word_model.c/.h`. Metadata comes from the actual TFLite interpreter. Hardware latency is left unset until measured. A real workflow still needs a documented audio feature extractor and representative recordings.

## Dashboard

Serve `www/` with any static server. It starts OFFLINE. Enter a `ws://`/`wss://` backend endpoint for live JSON or explicitly start deterministic simulation. Live payloads must use schema version 1; malformed and stale states are visible. Browser history is capped at 100 samples and CSV export preserves the live/simulation label.

## Tests

```sh
python -m compileall ml tests
python -m pytest
npm run lint
npm test
python scripts/secret_scan.py
python scripts/check_docs.py
```

Complete verification:

```powershell
./scripts/verify.ps1
```

```sh
./scripts/verify.sh
```

Use `-SkipIdf` / `--skip-idf` only when ESP-IDF is unavailable; the script then does not claim a firmware build. Use `-SkipMl` / `--skip-ml` only after separately recording the ML smoke result.

## Security limitations

This repository does not provision Wi-Fi, broker credentials, client certificates, Secure Boot, flash encryption, or eFuses. MQTT command authentication depends on the provisioned broker/TLS identity. BLE pairing/access-control policy still needs hardware threat-model validation. See [SECURITY.md](SECURITY.md).

## Troubleshooting

- `idf.py` not found: export the ESP-IDF environment (`export.ps1`, `export.bat`, or `export.sh`).
- Sensor ID mismatch: confirm voltage, pull-ups, pin mapping, and unshifted 7-bit addresses.
- No BLE advertising: enable both `CONFIG_BT_NIMBLE_ENABLED` and `CONFIG_AIOT_BLE_ENABLED`.
- MQTT rejected at startup: use TLS outside simulation and ensure the certificate bundle is enabled.
- TFLite conversion fails: use the exact pinned Python environment and a workspace-writable temporary directory.

Open limitations and the original audit are in [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).
