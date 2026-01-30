if(ENABLE_EXAMPLES)
  file(GLOB c_sources *.c)
  set_source_files_properties(${c_sources} PROPERTIES
    COMPILE_FLAGS "${WARNCFLAGS}")
  file(GLOB cxx_sources *.cc)
  set_source_files_properties(${cxx_sources} PROPERTIES
    COMPILE_FLAGS "${WARNCXXFLAGS} ${CXX1XCXXFLAGS}")

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/urlparse"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/llhttp/include"

    ${LIBEVENT_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
  )

  link_libraries(
    nghttp2
    ${LIBEVENT_OPENSSL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${APP_LIBRARIES}
  )

  add_executable(client           client.c $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )
  add_executable(libevent-client  libevent-client.c $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )
  add_executable(libevent-server  libevent-server.c $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )
  add_executable(deflate          deflate.c $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )
endif()
