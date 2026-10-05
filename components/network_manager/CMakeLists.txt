idf_component_register(SRCS "network_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common esp_wifi esp_netif esp_event nvs_flash)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
