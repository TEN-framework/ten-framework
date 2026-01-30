if(LIBBPF_FOUND)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/reuseport_kern.o"
    COMMAND ${CMAKE_C_COMPILER} ${BPFCFLAGS} ${EXTRABPFCFLAGS} -I${LIBBPF_INCLUDE_DIRS} -target bpf -c reuseport_kern.c -o "${CMAKE_CURRENT_BINARY_DIR}/reuseport_kern.o"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM)

  add_custom_target(bpf ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/reuseport_kern.o"
    VERBATIM)

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/reuseport_kern.o"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}")
endif()
