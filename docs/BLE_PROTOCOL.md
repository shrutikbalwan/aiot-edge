# BLE protocol

The optional transport is a NimBLE peripheral. All UUIDs use the base `10a10001-6f5d-d39a-884f-a4e200002cXX`.

| Item | UUID | Properties |
|---|---|---|
| AIoT service | `10a10001-6f5d-d39a-884f-a4e200002c7a` | Primary service |
| Device information | `10a10001-6f5d-d39a-884f-a4e200002c01` | Read |
| Sensor telemetry | `10a10001-6f5d-d39a-884f-a4e200002c02` | Notify |
| Wake-word event | `10a10001-6f5d-d39a-884f-a4e200002c03` | Notify |
| Device status | `10a10001-6f5d-d39a-884f-a4e200002c04` | Notify |
| Command input | `10a10001-6f5d-d39a-884f-a4e200002c05` | Write |
| OTA status | `10a10001-6f5d-d39a-884f-a4e200002c06` | Notify |

## Telemetry binary version 1

All multibyte fields are little-endian. No native C structure is transmitted.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Protocol version (`1`) |
| 1 | 1 | Flags: bit 0 simulated, bit 1 heart-rate valid, bit 2 SpO₂ valid |
| 2 | 4 | Timestamp milliseconds, low 32 bits |
| 6 | 4 | Red PPG |
| 10 | 4 | Infrared PPG |
| 14 | 4 | Signed temperature, millidegrees Celsius |
| 18 | 2 | Signed acceleration X, mg |
| 20 | 2 | Signed acceleration Y, mg |
| 22 | 2 | Signed acceleration Z, mg |
| 24 | 1 | Heart rate bpm, zero when invalid |

The peripheral checks that the 25-byte value fits `MTU - 3`, and notifies only on an active connection with telemetry subscription enabled.

## Commands

Writes are UTF-8 JSON capped at 512 bytes. Every command includes `"schema":1`. Supported validation forms are `status`, `reboot`, and an OTA trigger:

```json
{"schema":1,"command":"ota","url":"https://updates.example/firmware.bin","version":"1.2.3"}
```

An OTA URL must use HTTPS. The current characteristic validates and logs this candidate; it does not dispatch an update and is not BLE firmware transfer. Authentication/authorization policy must be added and validated for the product threat model before enabling remote actions.
