idf_component_register(SRCS "model_runtime.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
