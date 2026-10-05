# Known Issues and Verification Baseline

This document separates observed repository failures from work that requires physical hardware.

## Initial audit (2026-10-04)

The working tree was clean (`main...origin/main`) before the audit. Every tracked file was inventoried with `git ls-files`.

| Area | Initial evidence | Status |
|---|---|---|
| ESP-IDF build | Root `CMakeLists.txt` contained non-CMake text (`-- ...`), non-IDF commands, and ARMv7E-M flags. `cmake -S . -B baseline-cmake-build` exited 1 at line 4. | Verified failure |
| ESP-IDF tooling | `idf.py --version`, `idf.py set-target esp32s3`, and `idf.py build` each exited 1 because `idf.py` is not available in the current shell. | Environment limitation |
| Python syntax | `python -m compileall ml` exited 1 with `IndentationError` at `ml/train_model_complete.py:199`. | Verified failure |
| Local headers | Firmware referenced absent `system_config.h`, `audio_codec.h`, and `fft_lib.h`. | Verified failure |
| Entry point | Firmware used `main()` instead of ESP-IDF `app_main()`. | Verified failure |
| Firmware symbols | The code referenced undefined shared state, accelerator methods, firmware hash methods, and nonstandard I2C wrappers. | Verified failure |
| FreeRTOS lifecycle | Tasks could use resources before complete initialization; a binary semaphore was used as a mutex. | Verified failure |
| Sensors | Normal 7-bit addresses were shifted, BMI160 used the wrong address, I2C transactions were malformed, return codes were ignored, and PPG processing could read beyond supplied samples. | Verified failure |
| Bluetooth | Peripheral code mixed Classic Bluetooth, Bluedroid GATT client, and BLE APIs. | Verified failure |
| MQTT/TLS | Code targeted the retired Google service and set `skip_cert_common_name_check = true`. | Verified security failure |
| OTA | Code called undefined verification helpers and represented simulated transfer as secure update behavior. | Verified failure |
| ML | There were duplicate pipelines, fabricated performance fields, and an embedded header containing only a model fragment. | Verified failure |
| Dashboard | Simulation auto-started while the UI implied connection; JavaScript used invalid `var(--success)` expressions and inconsistent history records. | Verified failure |
| Tests/CI | No genuine automated test suite or CI workflow existed. The batch file was Markdown plus Bash and batch syntax. | Verified failure |
| Documentation | Completion, performance, ASIC, PUF, power, BLE, cloud, and security claims had no reproducible evidence. The README linked an absent license. | Verified failure |

## Remaining limitations

- Physical MAX30102, MAX30205, and BMI160 initialization and readings require board validation.
- BLE advertising, subscription, notification, MTU, and reconnect behavior require an ESP32-S3 and a BLE central.
- MQTT TLS interoperability and HTTPS OTA require provisioned Wi-Fi plus a real broker/server certificate chain.
- Secure Boot v2, flash encryption, anti-rollback, rollback confirmation, and eFuse provisioning require deliberate hardware provisioning and cannot be validated in host tests.
- Health values are educational estimates and are not clinically validated or suitable for diagnosis.
- Wake-word accuracy on real recordings and device inference latency remain unmeasured.
- MQTT/BLE command payloads are bounded and schema-validated, but action dispatch is intentionally disabled until a product authentication and authorization policy is implemented.
- The local WSL image uses Python 3.14 and has no compatible TensorFlow installation, so a full local `verify.sh` run stops at ML test collection. Its shell syntax passes; CI installs the pinned environment on Python 3.13. The Windows `verify.ps1` workflow passes locally.

The rewritten default firmware and the optional NimBLE/MQTT/OTA profile both build with the official ESP-IDF 5.1.6 container. The host shell still has no exported `idf.py`, so native invocation remains an environment limitation rather than a firmware failure. Current commands are maintained in the root `README.md` and verification scripts. Historical claims are not evidence.
