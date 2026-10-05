# Architecture

AIoT-Edge uses small ESP-IDF components with explicit ownership.

```text
sensor_driver task
  MAX30102 / MAX30205 / BMI160 / deterministic simulation
             |
             v (initialized aiot_sensor_sample_t, queue depth 1)
health processing task -- complete 100-sample window at explicit sample rate
             |                                      |
             v                                      v
      MQTT telemetry                         NimBLE telemetry

audio source (future hardware adapter) -> audio_features -> model_runtime -> wake event

MQTT command -> bounded schema parser -> TLS/credential gate -> command queue
                                                           -> status/OTA/reboot
BLE command  -> bounded schema parser -> authorization denied (no product ACL)
```

## Resource lifecycle

`app_main` initializes NVS, the sensor queue, event group, components, and only then starts tasks. Every allocation and task result is checked. The sensor task is priority 5 with a 4096-byte stack and runs every 100 ms using `xTaskDelayUntil`. The processing task is priority 4 with a 4096-byte stack and blocks on the queue. The OTA task, when enabled, is priority 3 with an 8192-byte stack and yields during download.

The sensor queue transfers values, not pointers. The producer owns a sample until `xQueueOverwrite` copies it; the consumer owns its received copy and window. OTA status is the only mutex-protected state. Network callbacks do bounded validation and enqueue authenticated MQTT commands without blocking. The dispatcher task owns execution. BLE writes use the same parser but are denied because the repository has no product BLE identity or ACL.

Wi-Fi station credentials and the MQTT password are loaded from a dedicated NVS namespace. Missing credentials leave networking offline without affecting local sensing. Disconnects schedule a one-shot timer with bounded exponential backoff. The provisioning functions are library integration points, not a remotely exposed provisioning service.

## Error behavior

I2C errors are returned and logged; no uninitialized sample is queued. Stale queue reads are logged. Transport unavailability does not stop local sensing. OTA transitions to failed with the ESP-IDF error and never reports readiness until image verification and boot-partition selection finish.

## Model runtime

The runtime interface is intentionally honest: the checked-in adapter is a CPU reference placeholder returning `ESP_ERR_NOT_SUPPORTED`. The generated complete model is suitable input for a future TFLite Micro/ESP-NN component, but no device inference or latency claim is made.
