/**
 * @file gcp_iot_config.h
 * @brief GCP IoT Core Configuration for AIoT-Edge
 * @brief TLS certificates, broker details, and device credentials
 * 
 * GENERATE YOUR OWN CERTIFICATES:
 * 1. Go to Google Cloud Console -> IoT Core -> Registry
 * 2. Create registry in us-east1 (or preferred region)
 * 3. Create device and download:
 *    - root CA (GCP root certificate)
 *    - device certificate (PEM encoded)
 *    - private key (PEM encoded)
 * 4. Paste contents below
 */

#ifndef GCP_IOT_CONFIG_H
#define GCP_IOT_CONFIG_H

#define MQTT_BROKER_URL "mqtts://mqtt.googleapis.com:8883"
#define MQTT_CLIENT_ID "aiot-edge-device-" __DATE__ "__TIME__"

#define MQTT_TOPIC_STATUS "aiot/edge/status"
#define MQTT_TOPIC_COMMANDS "aiot/edge/commands"
#define MQTT_TOPIC_OTA "aiot/edge/ota"

// ============================================================
// === TLS ROOT CA CERTIFICATE (GCP IoT Core) ============
// ============================================================
// Copy the GCP Root CA certificate here (BEGIN/END CERTIFICATE block)
// This is the Google Trust Services root CA for IoT Core

static const char ROOT_CA_PEM_START[] = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDXTCCAkWgAwIBAgIJAN[APP_CERT_HASH]\n"
    "... [41 base64 characters] ...\n"
    "-----END CERTIFICATE-----\n";

// ============================================================
// === DEVICE CERTIFICATE (Per-device identity) =========
// ============================================================
// Copy the specific device's certificate here

static const char DEVICE_CERT_PEM_START[] = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDXTCCAkWgAwIBAgIJAN[DEVICE_CERT_HASH]\n"
    "... [41 base64 characters] ...\n"
    "-----END CERTIFICATE-----\n";

// ============================================================
// === DEVICE PRIVATE KEY (Must match device cert) =======
// ============================================================
// Copy the device private key here (UNPROTECTED PEM format)

static const char DEVICE_PRIVATE_KEY_START[] = 
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAAS\n"
    "... [32 base64 characters] ...\n"
    "-----END PRIVATE KEY-----\n";

// ============================================================
// === MQTT CONNECTION SETTINGS ==========================
// ============================================================

/* TLS configuration for ESP-IDF MQTT client */
#define MQTT_TRANSPORT_OVER_TLS 1

/* Use true for self-signed GCP certs where CN may not match broker */
#define SKIP_CERT_COMMON_NAME_CHECK true

/* Heartbeat and keepalive settings */
#define MQTT_KEEPALIVE_SECONDS 60
#define MQTT_AUTO_RECONNECT true

/* Command processing settings */
#define MAX_COMMAND_QUEUE_LENGTH 4
#define HEARTBEAT_INTERVAL_SECONDS 30

/* OTA topic subscription */
#define OTA_COMMAND_TOPIC "aiot/edge/ota"

/* Federated Learning metadata */
#define FL_DELTA_TOPIC "aiot/edge/fl/delta"

/* Sensor telemetry rates */
#define SENSER_PUBLISH_INTERVAL_MS 5000  /* 2Hz telemetry */
#define BLE_SENSOR_PUBLISH_INTERVAL_MS 50  /* 20Hz BLE updates */

/* Wake word notification */
#define WAKE_WORD_TOPIC "aiot/edge/wake_word"

/* End of config */
#endif /* GCP_IOT_CONFIG_H */