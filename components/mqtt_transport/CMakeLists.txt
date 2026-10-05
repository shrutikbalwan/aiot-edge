idf_component_register(SRCS "mqtt_transport.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common mqtt mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
