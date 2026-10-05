idf_component_register(SRCS "command_dispatcher.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common ota esp_system)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
