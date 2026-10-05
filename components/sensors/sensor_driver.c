#include "sensor_driver.h"

#include <string.h>

#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"
#if CONFIG_AIOT_SIMULATION_RANDOM_DEMO
#include "esp_random.h"
#endif
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "signal_processing.h"

#define I2C_TIMEOUT_MS 100U
#define I2C_CLOCK_HZ 400000U
#define MAX30102_REG_FIFO_DATA 0x07U
#define MAX30102_REG_FIFO_CONFIG 0x08U
#define MAX30102_REG_MODE_CONFIG 0x09U
#define MAX30102_REG_SPO2_CONFIG 0x0AU
#define MAX30102_REG_LED1_PA 0x0CU
#define MAX30102_REG_LED2_PA 0x0DU
#define MAX30102_REG_PART_ID 0xFFU
#define MAX30102_PART_ID 0x15U
#define MAX30205_REG_TEMPERATURE 0x00U
#define MAX30205_REG_CONFIGURATION 0x01U
#define BMI160_REG_CHIP_ID 0x00U
#define BMI160_REG_ACCEL_DATA 0x12U
#define BMI160_REG_ACCEL_CONFIG 0x40U
#define BMI160_REG_ACCEL_RANGE 0x41U
#define BMI160_REG_COMMAND 0x7EU
#define BMI160_CHIP_ID 0xD1U

static const char *TAG = "sensors";
#if !CONFIG_AIOT_SIMULATION
static i2c_port_t s_i2c_port = (i2c_port_t)CONFIG_AIOT_I2C_PORT;
#endif
static uint32_t s_simulation_index;

#if !CONFIG_AIOT_SIMULATION
static esp_err_t write_register(uint8_t address, uint8_t reg, uint8_t value)
{
    const uint8_t bytes[2] = {reg, value};
    return i2c_master_write_to_device(s_i2c_port, address, bytes, sizeof(bytes),
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t read_registers(uint8_t address, uint8_t reg, uint8_t *data, size_t size)
{
    if (data == NULL || size == 0U) return ESP_ERR_INVALID_ARG;
    return i2c_master_write_read_device(s_i2c_port, address, &reg, 1U, data, size,
                                        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t validate_id(uint8_t address, uint8_t reg, uint8_t expected,
                             const char *device)
{
    uint8_t actual = 0;
    const esp_err_t err = read_registers(address, reg, &actual, 1U);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s ID read failed: %s", device, esp_err_to_name(err));
        return err;
    }
    if (actual != expected) {
        ESP_LOGE(TAG, "%s ID mismatch: expected 0x%02x, got 0x%02x", device,
                 expected, actual);
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static esp_err_t hardware_init(void)
{
    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_AIOT_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_AIOT_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_CLOCK_HZ,
        .clk_flags = 0,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(s_i2c_port, &config), TAG, "I2C config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(s_i2c_port, I2C_MODE_MASTER, 0, 0, 0),
                        TAG, "I2C install");
    ESP_RETURN_ON_ERROR(validate_id(MAX30102_I2C_ADDRESS, MAX30102_REG_PART_ID,
                                    MAX30102_PART_ID, "MAX30102"), TAG, "MAX30102");
    ESP_RETURN_ON_ERROR(write_register(MAX30102_I2C_ADDRESS, MAX30102_REG_MODE_CONFIG, 0x40U),
                        TAG, "MAX30102 reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(write_register(MAX30102_I2C_ADDRESS, MAX30102_REG_FIFO_CONFIG, 0x4FU),
                        TAG, "MAX30102 FIFO");
    ESP_RETURN_ON_ERROR(write_register(MAX30102_I2C_ADDRESS, MAX30102_REG_SPO2_CONFIG, 0x27U),
                        TAG, "MAX30102 sample config");
    ESP_RETURN_ON_ERROR(write_register(MAX30102_I2C_ADDRESS, MAX30102_REG_LED1_PA, 0x24U),
                        TAG, "MAX30102 red LED");
    ESP_RETURN_ON_ERROR(write_register(MAX30102_I2C_ADDRESS, MAX30102_REG_LED2_PA, 0x24U),
                        TAG, "MAX30102 IR LED");
    ESP_RETURN_ON_ERROR(write_register(MAX30102_I2C_ADDRESS, MAX30102_REG_MODE_CONFIG, 0x03U),
                        TAG, "MAX30102 mode");

    uint8_t temperature_config = 0;
    ESP_RETURN_ON_ERROR(read_registers(MAX30205_I2C_ADDRESS, MAX30205_REG_CONFIGURATION,
                                       &temperature_config, 1U), TAG, "MAX30205 probe");
    ESP_RETURN_ON_ERROR(write_register(MAX30205_I2C_ADDRESS, MAX30205_REG_CONFIGURATION, 0x00U),
                        TAG, "MAX30205 config");

    const uint8_t bmi_address = (uint8_t)CONFIG_AIOT_BMI160_ADDRESS;
    ESP_RETURN_ON_ERROR(validate_id(bmi_address, BMI160_REG_CHIP_ID, BMI160_CHIP_ID, "BMI160"),
                        TAG, "BMI160");
    ESP_RETURN_ON_ERROR(write_register(bmi_address, BMI160_REG_COMMAND, 0xB6U), TAG, "BMI160 reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(write_register(bmi_address, BMI160_REG_COMMAND, 0x11U), TAG, "BMI160 power");
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(write_register(bmi_address, BMI160_REG_ACCEL_CONFIG, 0x28U), TAG, "BMI160 rate");
    ESP_RETURN_ON_ERROR(write_register(bmi_address, BMI160_REG_ACCEL_RANGE, 0x03U), TAG, "BMI160 range");
    return ESP_OK;
}
#endif

esp_err_t sensor_driver_init(void)
{
    s_simulation_index = 0U;
#if CONFIG_AIOT_SIMULATION
    ESP_LOGW(TAG, "deterministic simulation enabled; values are not live measurements");
    return ESP_OK;
#else
    return hardware_init();
#endif
}

#if !CONFIG_AIOT_SIMULATION
static esp_err_t read_hardware(aiot_sensor_sample_t *sample)
{
    uint8_t ppg[6] = {0};
    uint8_t temperature[2] = {0};
    uint8_t acceleration[6] = {0};
    ESP_RETURN_ON_ERROR(read_registers(MAX30102_I2C_ADDRESS, MAX30102_REG_FIFO_DATA,
                                       ppg, sizeof(ppg)), TAG, "MAX30102 read");
    ESP_RETURN_ON_ERROR(read_registers(MAX30205_I2C_ADDRESS, MAX30205_REG_TEMPERATURE,
                                       temperature, sizeof(temperature)), TAG, "MAX30205 read");
    ESP_RETURN_ON_ERROR(read_registers((uint8_t)CONFIG_AIOT_BMI160_ADDRESS, BMI160_REG_ACCEL_DATA,
                                       acceleration, sizeof(acceleration)), TAG, "BMI160 read");
    sample->red = (((uint32_t)ppg[0] << 16U) | ((uint32_t)ppg[1] << 8U) | ppg[2]) & 0x3FFFFU;
    sample->infrared = (((uint32_t)ppg[3] << 16U) | ((uint32_t)ppg[4] << 8U) | ppg[5]) & 0x3FFFFU;
    const int16_t temp_raw = (int16_t)(((uint16_t)temperature[0] << 8U) | temperature[1]);
    sample->temperature_milli_c = ((int32_t)temp_raw * 1000) / 256;
    const int16_t x = (int16_t)(((uint16_t)acceleration[1] << 8U) | acceleration[0]);
    const int16_t y = (int16_t)(((uint16_t)acceleration[3] << 8U) | acceleration[2]);
    const int16_t z = (int16_t)(((uint16_t)acceleration[5] << 8U) | acceleration[4]);
    sample->accel_x_mg = (int16_t)(((int32_t)x * 1000) / 16384);
    sample->accel_y_mg = (int16_t)(((int32_t)y * 1000) / 16384);
    sample->accel_z_mg = (int16_t)(((int32_t)z * 1000) / 16384);
    return ESP_OK;
}
#endif

esp_err_t sensor_driver_read(aiot_sensor_sample_t *sample)
{
    if (sample == NULL) return ESP_ERR_INVALID_ARG;
    memset(sample, 0, sizeof(*sample));
    sample->timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000);
#if CONFIG_AIOT_SIMULATION
    static const int16_t pulse[10] = {0, 100, 400, 1000, 1800, 1000, 400, 100, 0, 0};
    const uint32_t index = s_simulation_index++;
    const uint32_t phase = index % 8U;
    sample->red = 48000U + (uint32_t)pulse[phase];
    sample->infrared = 52000U + (uint32_t)pulse[phase];
    sample->temperature_milli_c = 36500 + (int32_t)(index % 5U) * 10;
    sample->accel_x_mg = (int16_t)((int32_t)(index % 7U) - 3);
    sample->accel_y_mg = (int16_t)((int32_t)(index % 5U) - 2);
    sample->accel_z_mg = 1000;
#if CONFIG_AIOT_SIMULATION_RANDOM_DEMO
    /* Deliberately non-deterministic presentation noise; disabled in tests/defaults. */
    const int32_t noise = (int32_t)(esp_random() % 101U) - 50;
    sample->red = (uint32_t)((int32_t)sample->red + noise);
    sample->infrared = (uint32_t)((int32_t)sample->infrared + noise);
#endif
    sample->simulated = true;
    return ESP_OK;
#else
    return read_hardware(sample);
#endif
}

esp_err_t sensor_analyze_window(const aiot_sensor_sample_t *samples, size_t count,
                                uint32_t sample_rate_hz, aiot_health_estimate_t *estimate)
{
    if (samples == NULL || estimate == NULL || count != AIOT_PPG_WINDOW_SAMPLES) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(estimate, 0, sizeof(*estimate));
    uint32_t infrared[AIOT_PPG_WINDOW_SAMPLES] = {0};
    for (size_t i = 0; i < count; ++i) infrared[i] = samples[i].infrared;
    estimate->heart_rate_valid = signal_estimate_heart_rate(infrared, count,
                                                            sample_rate_hz,
                                                            &estimate->heart_rate_bpm);
    /* SpO2 and rhythm stay invalid until a clinically reviewed algorithm is supplied. */
    return ESP_OK;
}
