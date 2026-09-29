# AIoT-Edge Hardware Integration Checklist

## VLSI / AI Accelerator
- [ ] AI Accelerator ASIC power-on reset sequence
- [ ] Clock gating configuration for TPU block
- [ ] DMA setup for model weight transfer
- [ ] PUF (Physical Unclonable Function) initialization
- [ ] Side-channel countermeasures enable/disable
- [ ] Secure firmware update (SFU) bootloader integration
- [ ] Model hash verification (SHA-256) on every boot
- [ ] Power-gated design verification (<10mW idle, <5mW active)

## MCU / Firmware (ESP32-S3 or Cortex-M4)
- [ ] FreeRTOS v10.0+ task scheduling verified
- [ ] FPU enabled via `core_enable_fpu()` in system_init()
- [ ] TrustZone (if applicable) partition configuration
- [ ] Secure bootchain verification (cryptographic hash chain)
- [ ] watchdog timer configuration
- [ ] Power management: PMGR init + sleep modes
- [ ] I2C master bus initialized for all sensors
- [ ] UART for debug console (115200 baud)
- [ ] PWM for LED indicators / status lights

## Sensors (Analog/Digital I2C)
### PPG (MAX30102)
- [ ] I2C address: 0x57 >> 1 = 0x2C
- [ ] Reset sequence: write 0x40 to register 0x00
- [ ] Configuration: SpO2 + HR mode, LED current 50mA
- [ ] Sample rate: 100 SPS (register 0x25)
- [ ] Interrupt pin for new data available
- [ ] Red/IR sample reading (registers 0x06/0x07)

### Temperature (MAX30205)
- [ ] I2C address: 0x48 >> 1 = 0x24
- [ ] Configuration: Normal mode, 4Hz conversion rate
- [ ] Temperature LSB = 0.00390625°C
- [ ] Raw value = °C * 256 (divide by 256 for °C)
- [ ] Alert thresholds (high/low temperature)

### Accelerometer (BMI160)
- [ ] I2C address: 0x53 >> 1 = 0x29
- [ ] Soft reset: write 0xB6 to any register
- [ ] Power control: normal mode, 16g range
- [ ] Data rate: 1kHz ODR (register 0x1A = 0x0F)
- [ ] Full-scale range: ±16g
- [ ] Tap detection for fall detection
- [ ] Activity/inactivity interrupt

## Connectivity
### BLE 5.3
- [ ] Device name: "AIoT-Edge-HM"
- [ ] Advertising interval: 160 (100ms units = 160ms)
- [ ] Connection interval: 32 (20ms units = 20ms)
- [ ] Supervision timeout: 1000 (10s units = 10s)
- [ ] Maximum notification payload: 20 bytes
- [ ] GATT service: Heart Rate + Custom sensor data
- [ ] Alert characteristic: 1-byte type + optional payload
- [ ] OTA firmware check via BLE GATT write/notify

### MQTT over TLS (GCP IoT Core)
- [ ] Broker: mqtt.googleapis.com:8883
- [ ] Transport: MQTT over TLS (ESP_TLS)
- [ ] Root CA: GCP IoT Core root certificate
- [ ] Device certificate: Per-device X.509 certificate
- [ ] Private key: Must match device certificate
- [ ] Skip common name check: true (for self-signed GCP certs)
- [ ] Keepalive: 60 seconds
- [ ] Auto-reconnect: enabled
- [ ] Topics:
  - aiot/edge/status (telemetry, 2Hz)
  - aiot/edge/commands (firmware_update, nn_retrain, heartbeat_req)
  - aiot/edge/ota (delta firmware)
  - aiot/edge/wake_word (wake event notification)

## ML Inference
### Quantized Model
- [ ] TFLite model: wake_word.tflite (int8 quantized, ~12KB)
- [ ] Model metadata: metadata.json (accuracy, scaling, etc.)
- [ ] C header: nn_weights.h (embedded weights + scaling factors)
- [ ] Input: 256 uint8 features (normalized audio frame)
- [ ] Output: int8 confidence score (0-255 range)
- [ ] Scaling: INPUT_SCALING = 128.0f, OUTPUT_SCALING = 1.0/128.0f
- [ ] Inference latency: <2ms on AI accelerator, <10ms on CPU

### Inference Pipeline
- [ ] Audio processing task: 50Hz frame rate (20ms intervals)
- [ ] FFT: 512-point Hann windowed transform
- [ ] Feature extraction: MFCC-like from FFT output
- [ ] Normalization: input * (1/3.0) rescaling
- [ ] NN inference: ai_accelerator_infer(handle, input, output)
- [ ] Wake word threshold: acc_output[0] > 0.7f
- [ ] Wake word event: ble_transport_send_event(EVENT_WAKE_WORD)

## Inter-Task Communication (FreeRTOS)
### Queues
- [ ] g_sensor_data_queue: length 4, sizeof(edge_data_t)
- [ ] Data flow: Health Task → BLE Task (non-blocking xQueueSend)
- [ ] Timeout: pdMS_TO_TICKS(10) for xQueueReceive

### Semaphores
- [ ] g_sensor_semaphore: binary semaphore
- [ ] Use case: resource locking if needed

### Task Notifications
- [ ] NN Task → BLE Task: xTaskNotifyGive()
- [ ] Wake word flag: g_wake_word_detected shared variable
- [ ] Task priorities:
  - BLE Connectivity: priority 3 (highest)
  - Audio Processing: priority 2
  - NN Inference: priority 2
  - Health Monitoring: priority 1

## Power Management
### Power States
- [ ] Active mode: <5mW (all sensors + BLE + MCU)
- [ ] Standby mode: <10µW (RTC + watchdog only)
- [ ] Sleep modes: power-gated peripherals
- [ ] Wake sources: Timer, UART, GPIO, BLE event

### Power Sequencing
- [ ] Sensor power enable GPIO (if external LDO)
- [ ] Accelerometer sleep/wake control
- [ ] PPG shutdown when not measuring
- [ ] MCU deep sleep between sensor readings (10Hz fusion = 100ms intervals)

## Safety & Compliance
### ISO 26262 / ASIL Considerations
- [ ] Watchdog timer timeout configuration
- [ ] Watchdog refresh in main loop
- [ ] Fault detection and recovery tasks
- [ ] Error state machine (OK/FAIL/SAFE/RESET)
- [ ] Debug protection (JTAG disabled in production)

### Security
- [ ] PUF-based root of trust initialization
- [ ] Secure bootchain verification at startup
- [ ] OTA firmware signature validation
- [ ] No hardcoded secrets in production firmware
- [ ] Wipe/zeroize procedure for device decommissioning

## Testing & Verification
### Unit Tests
- [ ] Sensor read sanity checks (min/max values)
- [ ] I2C bus scan and device detection
- [ ] BLE advertising scan response verification
- [ ] MQTT connect/disconnect cycle testing
- [ ] MQTT command processing (firmware_update, nn_retrain, heartbeat)

### Integration Tests
- [ ] Full sensor fusion: PPG + Temp + Accel data read
- [ ] BLE data transmission to phone app
- [ ] MQTT telemetry publish to GCP IoT Core
- [ ] Wake word detection via audio pipeline
- [ ] Irregular rhythm detection and alert trigger
- [ ] OTA firmware update flow (simulated)

### Performance Benchmarks
- [ ] Sensor read cycle: <50ms (10Hz fusion rate)
- [ ] Audio frame processing: <20ms (50Hz rate)
- [ ] NN inference: <2ms (AI accelerator), <10ms (CPU)
- [ ] BLE notification latency: <100ms
- [ ] MQTT publish latency: <500ms (including TLS handshake)

## Deployment Checklist
### Before Production
- [ ] All unit tests passing
- [ ] All integration tests passing  
- [ ] Performance benchmarks met
- [ ] Security review completed
- [ ] PUF enrollment on each device
- [ ] Unique device certificates provisioned
- [ ] OTA firmware v1.0 released to cloud
- [ ] Documentation updated (README, API specs)
- [ ] Code review completed (all critical bugs fixed)

### Production Release
- [ ] Flash firmware to all devices
- [ ] Provision GCP IoT Core certificates per device
- [ ] Verify PUF roots of trust match
- [ ] Monitor initial telemetry flow
- [ ] Watch watchdog reset patterns
- [ ] Log analysis for anomaly detection
- [ ] Schedule periodic firmware updates via OTA