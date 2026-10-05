idf_component_register(SRCS "audio_features.c"
                       INCLUDE_DIRS "include"
                       REQUIRES aiot_common)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
