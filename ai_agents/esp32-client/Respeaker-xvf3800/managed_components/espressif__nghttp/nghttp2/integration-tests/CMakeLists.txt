set(GO_FILES
  nghttpx_http1_test.go
  nghttpx_http2_test.go
  server_tester.go
  server_tester_http3.go
)

# XXX unused
set(EXTRA_DIST
  ${GO_FILES}
  server.key
  server.crt
  alt-server.key
  alt-server.crt
  setenv
  req-set-header.rb
  resp-set-header.rb
  req-return.rb
  resp-return.rb
)

# 'go test' requires both config.go and the test files in the same directory.
# For out-of-tree builds, config.go is normally not placed next to the source
# files, so copy the tests to the build directory as a workaround.
set(GO_BUILD_FILES)
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  foreach(gofile IN LISTS GO_FILES)
    set(outfile "${CMAKE_CURRENT_BINARY_DIR}/${gofile}")
    add_custom_command(OUTPUT "${outfile}"
      COMMAND ${CMAKE_COMMAND} -E copy
              "${CMAKE_CURRENT_SOURCE_DIR}/${gofile}" "${outfile}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${gofile}"
    )
    list(APPEND GO_BUILD_FILES "${outfile}")
  endforeach()
endif()

if(ENABLE_HTTP3)
  set(GO_TEST_TAGS quic)
endif()

add_custom_target(it
  COMMAND sh setenv go test -v --tags=${GO_TEST_TAGS}
  DEPENDS ${GO_BUILD_FILES}
)
