idf_component_register(SRCS "ota_manager.c" "ota_policy.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common app_update esp_https_ota esp_app_format mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
