idf_component_register(SRCS "ble_transport.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common bt)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
