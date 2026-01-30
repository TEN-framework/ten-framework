file(GLOB c_sources *.c)
set_source_files_properties(${c_sources} PROPERTIES
  COMPILE_FLAGS "${WARNCFLAGS}")
file(GLOB cxx_sources *.cc)
set_source_files_properties(${cxx_sources} PROPERTIES
  COMPILE_FLAGS "${WARNCXXFLAGS} ${CXX1XCXXFLAGS}")

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/includes"
  "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/urlparse"
  "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/llhttp/include"

  ${JEMALLOC_INCLUDE_DIRS}
  ${LIBXML2_INCLUDE_DIRS}
  ${LIBEV_INCLUDE_DIRS}
  ${LIBNGHTTP3_INCLUDE_DIRS}
  ${LIBNGTCP2_INCLUDE_DIRS}
  ${LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIRS}
  ${LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIRS}
  ${WOLFSSL_INCLUDE_DIRS}
  ${LIBCARES_INCLUDE_DIRS}
  ${JANSSON_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
  ${LIBBPF_INCLUDE_DIRS}
  ${LIBBROTLIENC_INCLUDE_DIRS}
  ${LIBBROTLIDEC_INCLUDE_DIRS}
)

# XXX per-target?
link_libraries(
  nghttp2
  ${JEMALLOC_LIBRARIES}
  ${LIBXML2_LIBRARIES}
  ${LIBEV_LIBRARIES}
  ${LIBNGHTTP3_LIBRARIES}
  ${LIBNGTCP2_LIBRARIES}
  ${LIBNGTCP2_CRYPTO_QUICTLS_LIBRARIES}
  ${LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${WOLFSSL_LIBRARIES}
  ${LIBCARES_LIBRARIES}
  ${JANSSON_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${APP_LIBRARIES}
  ${LIBBPF_LIBRARIES}
  ${LIBBROTLIENC_LIBRARIES}
  ${LIBBROTLIDEC_LIBRARIES}
)

if(ENABLE_APP)
  set(HELPER_OBJECTS
    util.cc
    http2.cc timegm.c app_helper.cc nghttp2_gzip.c
  )

  # nghttp client
  set(NGHTTP_SOURCES
    ${HELPER_OBJECTS}
    nghttp.cc
    tls.cc
  )
  if(HAVE_LIBXML2)
    list(APPEND NGHTTP_SOURCES HtmlParser.cc)
  endif()

  # nghttpd
  set(NGHTTPD_SOURCES
    ${HELPER_OBJECTS}
    nghttpd.cc
    tls.cc
    HttpServer.cc
  )

  # h2load
  set(H2LOAD_SOURCES
    util.cc
    http2.cc h2load.cc
    timegm.c
    tls.cc
    h2load_http2_session.cc
    h2load_http1_session.cc
  )
  if(ENABLE_HTTP3)
    list(APPEND H2LOAD_SOURCES
      h2load_http3_session.cc
      h2load_quic.cc
      quic.cc
    )
  endif()

  # Common libnhttpx sources (used for nghttpx and unit tests)
  set(NGHTTPX_SRCS
    util.cc http2.cc timegm.c
    app_helper.cc
    tls.cc
    shrpx_config.cc
    shrpx_accept_handler.cc
    shrpx_connection_handler.cc
    shrpx_client_handler.cc
    shrpx_http2_upstream.cc
    shrpx_https_upstream.cc
    shrpx_downstream.cc
    shrpx_downstream_connection.cc
    shrpx_http_downstream_connection.cc
    shrpx_http2_downstream_connection.cc
    shrpx_http2_session.cc
    shrpx_downstream_queue.cc
    shrpx_log.cc
    shrpx_http.cc
    shrpx_io_control.cc
    shrpx_tls.cc
    shrpx_worker.cc
    shrpx_log_config.cc
    shrpx_connect_blocker.cc
    shrpx_live_check.cc
    shrpx_downstream_connection_pool.cc
    shrpx_rate_limit.cc
    shrpx_connection.cc
    shrpx_memcached_dispatcher.cc
    shrpx_memcached_connection.cc
    shrpx_worker_process.cc
    shrpx_signal.cc
    shrpx_router.cc
    shrpx_api_downstream_connection.cc
    shrpx_health_monitor_downstream_connection.cc
    shrpx_null_downstream_connection.cc
    shrpx_exec.cc
    shrpx_dns_resolver.cc
    shrpx_dual_dns_resolver.cc
    shrpx_dns_tracker.cc
    xsi_strerror.c
  )
  if(HAVE_MRUBY)
    list(APPEND NGHTTPX_SRCS
      shrpx_mruby.cc
      shrpx_mruby_module.cc
      shrpx_mruby_module_env.cc
      shrpx_mruby_module_request.cc
      shrpx_mruby_module_response.cc
    )
  endif()
  if(ENABLE_HTTP3)
    list(APPEND NGHTTPX_SRCS
     shrpx_quic.cc
     shrpx_quic_listener.cc
     shrpx_quic_connection_handler.cc
     shrpx_http3_upstream.cc
     http3.cc
     quic.cc
     siphash.cc
    )
  endif()
  add_library(nghttpx_static STATIC ${NGHTTPX_SRCS})
  set_target_properties(nghttpx_static PROPERTIES ARCHIVE_OUTPUT_NAME nghttpx)

  set(NGHTTPX-bin_SOURCES
    shrpx.cc
  )

  if(HAVE_SYSTEMD)
    target_link_libraries(nghttpx_static ${SYSTEMD_LIBRARIES})
    target_compile_definitions(nghttpx_static PUBLIC HAVE_LIBSYSTEMD)
    target_include_directories(nghttpx_static PUBLIC ${SYSTEMD_INCLUDE_DIRS})
  endif()

  if(HAVE_MRUBY)
    target_link_libraries(nghttpx_static mruby-lib)
  endif()

  if(HAVE_NEVERBLEED)
    target_link_libraries(nghttpx_static neverbleed)
  endif()

  if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    set(NGHTTPX_UNITTEST_SOURCES
      shrpx-unittest.cc
      shrpx_tls_test.cc
      shrpx_downstream_test.cc
      shrpx_config_test.cc
      shrpx_worker_test.cc
      shrpx_http_test.cc
      shrpx_router_test.cc
      http2_test.cc
      util_test.cc
      nghttp2_gzip_test.c
      nghttp2_gzip.c
      buffer_test.cc
      memchunk_test.cc
      template_test.cc
      base64_test.cc
      ${CMAKE_SOURCE_DIR}/tests/munit/munit.c
    )
    if(ENABLE_HTTP3)
      list(APPEND NGHTTPX_UNITTEST_SOURCES siphash_test.cc)
    endif()
    add_executable(nghttpx-unittest EXCLUDE_FROM_ALL
      ${NGHTTPX_UNITTEST_SOURCES}
      $<TARGET_OBJECTS:llhttp>
      $<TARGET_OBJECTS:urlparse>
    )
    target_include_directories(nghttpx-unittest PRIVATE
      ${CMAKE_SOURCE_DIR}/tests/munit
    )
    target_compile_definitions(nghttpx-unittest
      PRIVATE "-DNGHTTP2_SRC_DIR=\"${CMAKE_SOURCE_DIR}/src\""
    )
    target_link_libraries(nghttpx-unittest nghttpx_static)
    if(HAVE_MRUBY)
      target_link_libraries(nghttpx-unittest mruby-lib)
    endif()
    if(HAVE_NEVERBLEED)
      target_link_libraries(nghttpx-unittest neverbleed)
    endif()

    add_test(nghttpx-unittest nghttpx-unittest)
    add_dependencies(check nghttpx-unittest)
  endif()

  add_executable(nghttp   ${NGHTTP_SOURCES}   $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )
  add_executable(nghttpd  ${NGHTTPD_SOURCES}  $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )
  add_executable(nghttpx  ${NGHTTPX-bin_SOURCES} $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )
  target_compile_definitions(nghttpx PRIVATE
    "-DPKGDATADIR=\"${PKGDATADIR}\""
    "-DPKGLIBDIR=\"${PKGLIBDIR}\""
  )
  target_link_libraries(nghttpx nghttpx_static)
  add_executable(h2load   ${H2LOAD_SOURCES}   $<TARGET_OBJECTS:llhttp>
    $<TARGET_OBJECTS:urlparse>
  )

  install(TARGETS nghttp nghttpd nghttpx h2load)
endif()

if(ENABLE_HPACK_TOOLS)
  set(inflatehd_SOURCES
    inflatehd.cc
    comp_helper.c
  )
  set(deflatehd_SOURCES
    deflatehd.cc
    comp_helper.c
    util.cc
    timegm.c
  )
  add_executable(inflatehd ${inflatehd_SOURCES})
  add_executable(deflatehd ${deflatehd_SOURCES})
  install(TARGETS inflatehd deflatehd)
endif()
