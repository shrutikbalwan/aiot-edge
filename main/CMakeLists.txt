idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES aiot_common sensors audio model_runtime ble_transport mqtt_transport ota nvs_flash)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type -Werror=implicit-function-declaration)
