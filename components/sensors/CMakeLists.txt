idf_component_register(SRCS "sensor_driver.c" "signal_processing.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common driver esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wconversion -Werror=return-type)
