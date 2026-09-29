cmake_minimum_required(VERSION 3.16)
project(AIoT-Edge IDF_VER v5.1.0)

-- ESP-IDF component definitions

idf_component_requirements(
  esp-idf >= "v5.1.0"
)

-- Component dependencies
requires(
  freertos
  spirv_toolchain
)

-- Component sources

aux_source_directory(. SOURCES)

-- Source file grouping

target_include_paths(
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/fw
  ${CMAKE_CURRENT_SOURCE_DIR}/iot
  ${CMAKE_CURRENT_SOURCE_DIR}/ml
  ${CMAKE_CURRENT_SOURCE_DIR}/hw
)

target_compile_definitions(
  PRIVATE
  SAMPLE_RATE=16000
  FRAME_SIZE=256
  FFT_SIZE=512
  NUM_SENSOR_SAMPLES=128
)

-- Compiler optimizations for Cortex-M4

target_compile_options(
  PRIVATE
  -march=armv7e-m
  -mfloat-abi=hard
  -mfpu=fpv4-sp-d16
  -O2
  -ffunction-sections
  -fdata-sections
)

-- Linker optimizations

target_link_options(
  PRIVATE
  -Wl,--gc-sections
)

-- Component libs

target_libs(
  PRIVATE
  esp_wifi
  esp_bluedroid
  esp_bt
  nvs_flash
  esp_timer
  esp_event
  freertos
  mcuboot
)