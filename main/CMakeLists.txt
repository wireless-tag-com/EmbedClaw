idf_component_register(
    SRC_DIRS
       "."
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        spiffs
        embed_claw
)

set(EC_APP_CONFIG_HEADER "${CMAKE_CURRENT_LIST_DIR}/ec_config.h")
if(EXISTS "${EC_APP_CONFIG_HEADER}")
    idf_component_get_property(embed_claw_lib embed_claw COMPONENT_LIB)
    target_compile_options(${embed_claw_lib} PRIVATE
        "-include"
        "${EC_APP_CONFIG_HEADER}"
    )
endif()
