# AIoT-Edge

AIoT-Edge is an ESP32-S3 ESP-IDF project for deterministic sensor simulation, educational health-signal estimates, BLE telemetry, provider-neutral MQTT, verified HTTPS OTA, a TinyML training/export workflow, and a browser dashboard.

Health outputs are engineering demonstrations, not medical devices or diagnoses. No repository result establishes clinical accuracy.

## Current status

| Area | Status | Evidence or boundary |
|---|---|---|
| Host signal processing | Verified | Native C test covers nominal, short, null, and flat windows. |
| Python/ML smoke pipeline | Verified | Pytest converts a full-int8 model, executes TFLite inference, and compiles the generated complete C array. |
| Dashboard and telemetry relay | Verified | Node syntax checks and six unit tests cover parsing, bounded history, relay configuration, authorization, and payload limits. |
| ESP32-S3 project structure | Verified | Clean default build completed with the official `espressif/idf:v5.1.6` environment; optional NimBLE/MQTT/OTA configuration also compiles. |
| MAX30102/MAX30205/BMI160 drivers | Implemented, hardware validation pending | Correct 7-bit addressing and bounded decoding; physical buses are untested. |
| Wi-Fi, NimBLE, MQTT commands, HTTPS OTA | Implemented, hardware validation pending | Optional and disabled by default; runtime credential APIs exist, but product provisioning and interoperability require hardware/network testing. |
| CPU model-runtime adapter | Experimental | Interface exists; a production TFLite Micro/ESP-NN backend is not bundled. |
| Real wake-word dataset and device benchmark | Planned | Synthetic accuracy is only a pipeline smoke result. |

No custom ASIC, accelerator RTL, synthesis, timing closure, PUF implementation, or side-channel evaluation exists here.

## Architecture

```text
Sensors -> sensor queue -> health processing -> MQTT/BLE telemetry
Audio   -> feature extraction -> model runtime -> wake event
MQTT commands -> bounded validation -> authenticated dispatcher -> status/OTA/reboot
BLE commands  -> bounded validation -> rejected until product BLE authorization exists
```

Resources are created before tasks. The sensor task owns sensor reads, the queue transfers initialized samples, and the processing task owns its complete PPG window. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Hardware

- ESP32-S3 only
- 4 MB or larger flash (the default partition table provides two 1,966,080-byte OTA slots with rollback metadata; there is no separate factory slot)
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

Use `idf.py menuconfig` under **AIoT-Edge** to disable simulation, choose the BMI160 address, or enable optional transports. Enabling NimBLE also requires the ESP-IDF NimBLE host option. Copy values from `config/examples/sdkconfig.defaults.local.example`; do not commit a generated `sdkconfig` containing secrets. `network_manager_provision_wifi()` and `mqtt_transport_provision_password()` are integration APIs for a trusted local/manufacturing provisioning flow; this repository intentionally does not expose credentials through BLE, HTTP, or a shell.

## Simulation and hardware setup

Simulation is deterministic by default and is explicitly logged. It generates a repeatable pulse and stationary acceleration. It does not prove sensor correctness.

For hardware, disable `CONFIG_AIOT_SIMULATION`, connect 3.3 V/GND and I2C with appropriate pull-ups, verify addresses with a bus analyzer or scanner, and validate output against trusted references. MAX30205 lacks a chip-ID register, so initialization checks bus communication/configuration; MAX30102 and BMI160 IDs are checked.

## MQTT

Topics are versioned:

- `aiot/v1/devices/{device_id}/telemetry` — QoS 1, not retained
- `aiot/v1/devices/{device_id}/events` — QoS 1, not retained (reserved)
- `aiot/v1/devices/{device_id}/commands` — QoS 1 subscription; publishers should not retain commands
- `aiot/v1/devices/{device_id}/ota/status` — QoS 1 (reserved)

For local Mosquitto development, simulation builds may use `mqtt://<LAN-IP>:1883`. Production configuration must use `mqtts://`; the code attaches the ESP certificate bundle and does not bypass hostname checks. Usernames can come from Kconfig for development, while the password is read from NVS. Command subscription requires TLS, a username, a runtime password, and `CONFIG_AIOT_COMMANDS_ENABLED`; broker ACLs must restrict the command topic to authorized publishers. The dashboard consumes the included authenticated backend WebSocket relay rather than embedding broker credentials; see [docs/MQTT_AND_DASHBOARD.md](docs/MQTT_AND_DASHBOARD.md).

## BLE and OTA

NimBLE exposes explicit, versioned binary telemetry and JSON command candidates. Notifications occur only while connected and subscribed, and payload size is checked against the negotiated MTU. Firmware bytes are not transferred over BLE. BLE commands are schema-validated and then rejected because transport encryption alone is not product authorization. See [docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md).

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

Run `npm run bridge` with `AIOT_MQTT_URL`, a URL-safe 32-128 character `AIOT_WS_TOKEN`, and any broker credentials in the process environment. The relay binds to `127.0.0.1` by default; put it behind a TLS reverse proxy for remote browser access. Serve `www/` with any static server, enter the resulting `ws://`/`wss://` `/telemetry` endpoint and relay token, or explicitly start deterministic simulation. The page starts OFFLINE, never stores the token, rejects simulated or malformed live payloads, marks stale data, caps local history at 100 samples, and labels CSV rows by source.

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

This repository provides NVS storage APIs but not an end-user provisioning channel. Without ESP32-S3 flash encryption, NVS credentials are not protected against physical flash extraction. It also does not provision client certificates, Secure Boot, flash encryption, or eFuses. MQTT command authorization depends on the provisioned broker identity and broker ACLs. BLE command execution remains disabled pending a product-specific pairing and authorization design. See [SECURITY.md](SECURITY.md).

## Troubleshooting

- `idf.py` not found: export the ESP-IDF environment (`export.ps1`, `export.bat`, or `export.sh`).
- Sensor ID mismatch: confirm voltage, pull-ups, pin mapping, and unshifted 7-bit addresses.
- No BLE advertising: enable both `CONFIG_BT_NIMBLE_ENABLED` and `CONFIG_AIOT_BLE_ENABLED`.
- MQTT rejected at startup: use TLS outside simulation and ensure the certificate bundle is enabled.
- TFLite conversion fails: use the exact pinned Python environment and a workspace-writable temporary directory.

Open limitations and the original audit are in [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).
