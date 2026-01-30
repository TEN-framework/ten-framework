# XXX testdata/: EXTRA_DIST = cacert.pem  index.html  privkey.pem
string(REPLACE " " ";" c_flags "${WARNCFLAGS}")
add_compile_options(${c_flags})

include_directories(
  "${CMAKE_SOURCE_DIR}/lib/includes"
  "${CMAKE_SOURCE_DIR}/lib"
  "${CMAKE_SOURCE_DIR}/tests/munit"
  "${CMAKE_BINARY_DIR}/lib/includes"
)

set(MAIN_SOURCES
  main.c nghttp2_pq_test.c nghttp2_map_test.c nghttp2_queue_test.c
  nghttp2_test_helper.c
  nghttp2_frame_test.c
  nghttp2_stream_test.c
  nghttp2_session_test.c
  nghttp2_hd_test.c
  nghttp2_alpn_test.c
  nghttp2_helper_test.c
  nghttp2_buf_test.c
  nghttp2_http_test.c
  nghttp2_extpri_test.c
  nghttp2_ratelim_test.c
  munit/munit.c
)

add_executable(main EXCLUDE_FROM_ALL
  ${MAIN_SOURCES}
)
target_link_libraries(main
  nghttp2_static
)
add_test(main main)
add_dependencies(check main)

if(ENABLE_FAILMALLOC)
  set(FAILMALLOC_SOURCES
    failmalloc.c failmalloc_test.c
    malloc_wrapper.c
    nghttp2_test_helper.c
    munit/munit.c
  )
  add_executable(failmalloc EXCLUDE_FROM_ALL
    ${FAILMALLOC_SOURCES}
  )
  target_link_libraries(failmalloc
    nghttp2_static
  )
  add_test(failmalloc failmalloc)
  add_dependencies(check failmalloc)
endif()
