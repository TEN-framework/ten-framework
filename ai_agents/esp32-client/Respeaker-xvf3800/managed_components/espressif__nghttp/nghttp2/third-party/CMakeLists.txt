if(ENABLE_THIRD_PARTY)
  set(LIBLLHTTP_SOURCES
    llhttp/src/api.c
    llhttp/src/http.c
    llhttp/src/llhttp.c
  )
  add_library(llhttp OBJECT ${LIBLLHTTP_SOURCES})
  target_include_directories(llhttp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/llhttp/include"
  )
  set_target_properties(llhttp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  set(LIBURLPARSE_SOURCES
    urlparse/urlparse.c
  )
  add_library(urlparse OBJECT ${LIBURLPARSE_SOURCES})
  set_target_properties(urlparse PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

  if(HAVE_NEVERBLEED)
    set(NEVERBLEED_SOURCES
      neverbleed/neverbleed.c
    )
    add_library(neverbleed ${NEVERBLEED_SOURCES})
    target_include_directories(neverbleed PRIVATE ${OPENSSL_INCLUDE_DIRS})
    target_include_directories(neverbleed INTERFACE
      "${CMAKE_SOURCE_DIR}/third-party/neverbleed"
    )
    target_link_libraries(neverbleed ${OPENSSL_LIBRARIES})
    target_compile_definitions(neverbleed PUBLIC _GNU_SOURCE)
  endif()

  if(HAVE_MRUBY)
    # EXTRA_DIST = build_config.rb mruby/*

    set(MRUBY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/mruby/build")
    set(MRUBY_LIBRARY
      "${CMAKE_STATIC_LIBRARY_PREFIX}mruby${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )

    # The mruby build needs some env vars. Alternatively, look at cmake -P
    if(CMAKE_VERSION VERSION_LESS "3.1")
      # XXX works only for Unixes?
      set(ENV_COMMAND env)
    else()
      set(ENV_COMMAND ${CMAKE_COMMAND} -E env)
    endif()
    # Required for the Ninja generator. For older CMake, you first have to
    # invoke 'ninja mruby' before building dependents.
    if(CMAKE_VERSION VERSION_LESS "3.2")
      set(_byproducts)
    else()
      set(_byproducts BYPRODUCTS "mruby/build/lib/${MRUBY_LIBRARY}")
    endif()
    add_custom_target(mruby
      COMMAND ${ENV_COMMAND}
        "MRUBY_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/build_config.rb"
        "BUILD_DIR=${MRUBY_BUILD_DIR}"
        "INSTALL_DIR=${MRUBY_BUILD_DIR}/install/bin"
        "MRUBY_CC=${CMAKE_C_COMPILER}" "MRUBY_CXX=${CMAKE_CXX_COMPILER}"
        "${CMAKE_CURRENT_SOURCE_DIR}/mruby/minirake"
        -f "${CMAKE_CURRENT_SOURCE_DIR}/mruby/Rakefile"
      ${_byproducts}
      VERBATIM
    )

    # Make the mruby library available to others in this project without them
    # having to worry about include dirs and the mruby location.
    add_library(mruby-lib STATIC IMPORTED GLOBAL)
    set_target_properties(mruby-lib PROPERTIES
      IMPORTED_LOCATION "${MRUBY_BUILD_DIR}/lib/${MRUBY_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/mruby/include"
    )
    add_dependencies(mruby-lib mruby)

    set_directory_properties(PROPERTIES
      ADDITIONAL_MAKE_CLEAN_FILES mruby/build
    )
  endif()
endif()
