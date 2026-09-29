# AIoT-Edge: Intelligent Voice-Health Monitoring Device

## Project Overview
A comprehensive VLSI + Embedded Systems + IoT + AI/ML integrated project demonstrating edge intelligence for health monitoring.

## Architecture Domains

### 1. VLSI Hardware
- Custom AI Accelerator ASIC with Tensor Processing Unit
- Hardware-accelerated neural network inference (8-bit quantized)
- PUF-based security root of trust
- Power-gated design (<10mW idle)

### 2. Embedded Firmware
- FreeRTOS with safety-critical task scheduling
- ARM Cortex-M4 with FPU and TrustZone
- Secure bootchain with cryptographic verification
- OTA update mechanism with signed firmware

### 3. IoT Infrastructure
- MQTT over TLS to Google Cloud IoT Core
- 5G NR connectivity with network slicing
- Blockchain-secured firmware updates (Hyperledger)
- IPFS decentralized storage integration

### 4. Edge AI/ML
- TinyML wake word detection (<50KB quantized model)
- Federated learning across device fleet
- Multimodal sensor fusion (audio + vital signs)
- Neural architecture search for edge optimization

## Repository Structure

```
AIoT-Edge/
├─ hw/           # VLSI/ASIC design (Verilog/VHDL)
│   └─ ai_accelerator.h  # AI accelerator interface
│
├─ fw/           # Embedded firmware
│   ├─ main.c            # FreeRTOS application entry
│   ├─ sensor_drivers.h  # PPG, temp, accelerometer drivers
│   └─ ...               # Additional firmware modules
│
├─ ml/           # TinyML models and training
│   ├─ train_model.py    # Model training & quantization
│   └─ models/           # Exported .tflite models
│
├─ iot/          # Cloud connectivity
│   ├─ mqtt_client.c     # MQTT over TLS, OTA, commands
│   └─ ...               # Additional IoT modules
│
└─ docs/         # Project documentation
    └─ README.md
```

## Quick Start

### Prerequisites
- ESP32-S3 or similar Cortex-M4+ capable MCU
- TensorFlow Lite for Microcontrollers
- FreeRTOS v10.0+
- MQTT broker access (Google Cloud IoT Core recommended)

### Building Firmware
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

### Training TinyML Model
```bash
python3 ml/train_model.py
```
Exports `ml/models/wake_word.tflite` (~12KB after quantization)

### IoT Cloud Setup
1. Create Google Cloud IoT Core registry
2. Configure TLS certificates
3. Update `iot/mqtt_client.c` with broker details
4. Run `iot_main()` to establish connection

## Performance Targets
- **Inference latency**: < 2ms on AI accelerator
- **Model size**: < 50 KB (int8 quantized)
- **Power consumption**: < 5mW active, < 10µW standby
- **Connectivity**: 5G NR, BLE 5.3, MQTT over TLS
- **Security**: ISO 26262 ASIL-D compatible, PUF-based root of trust

## Key Innovations
1. Hardware-accelerated TinyML on custom VLSI
2. Federated learning without raw data leaving device
3. Edge-edge collaboration via IPFS/Blockchain
4. Multi-source energy harvesting management
5. Post-quantum cryptography ready

## Academic / Industry Alignment
- Relevant to: Edge AI, IoT Security, VLSI Design, Embedded Systems
- Skills demonstrated: RTL design, Firmware engineering, Cloud architecture, Model compression
- Recruitment value: High - covers full-stack embedded+AI domain