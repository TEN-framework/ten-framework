execute_process(COMMAND ${MBEDTLS_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/config.py -f ${CMAKE_CURRENT_SOURCE_DIR}/../include/mbedtls/mbedtls_config.h get MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED RESULT_VARIABLE result)

if(${result} EQUAL 0)
    add_subdirectory(everest)
endif()
