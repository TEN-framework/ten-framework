idf_component_register(INCLUDE_DIRS "include")

if(CONFIG_JSMN_PARENT_LINKS)
    target_compile_definitions(${COMPONENT_LIB} INTERFACE "-DJSMN_PARENT_LINKS")
endif()

if(CONFIG_JSMN_STRICT)
    target_compile_definitions(${COMPONENT_LIB} INTERFACE "-DJSMN_STRICT")
endif()

if(CONFIG_JSMN_STATIC)
    target_compile_definitions(${COMPONENT_LIB} INTERFACE "-DJSMN_STATIC")
endif()
