idf_component_register(SRCS "aiot_serialization.c" "credential_policy.c"
                       INCLUDE_DIRS "include"
                       REQUIRES json)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
