
if(OPENSSL_FOUND)

	find_program(OPENSSL_EXECUTABLE openssl openssl.exe bin/openssl.exe
		HINTS ${_OPENSSL_ROOT_HINTS}
		PATH 
			/usr/bin/ 
			bin/
	  	DOC "Openssl executable")

	mark_as_advanced(OPENSSL_EXECUTABLE)
	
	# On Windows, we need to copy the OpenSSL dlls 
	# to the output directory.
  # BUT only if non-static libs (referencing dlls) are used
  # In this case
  # ** we only want to find dlls that are compatible with the libs
  #    the assumption is that these are part of the same OpenSSL package
  #    and typically reside in the same or in a close by directory as the executable
  # ** we do NOT want to find dlls in general dll directories such as C:\Windows\systemXX
  #    because these IN GENERAL are not compatible with the libs
	if (WIN32 AND OPENSSL_VERSION)
		set(OPENSSL_BIN_FOUND 0)

    # we check for OpenSSL versioning, as described in https://wiki.openssl.org/index.php/Versioning
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.(.*)$" REGEX_MATCH ${OPENSSL_VERSION})

    if (NOT ${REGEX_MATCH} EQUAL "")

      message(DEBUG "Assuming OpenSSL release ${OPENSSL_VERSION} >= 1.1.0 for dll discovery")

      # the regex matched - so we assume OpenSSL release >= 1.1
      set(OVNR "${CMAKE_MATCH_1}") # OpenSSL version number
      set(ORNR "${CMAKE_MATCH_2}") # OpenSSL release number
      set(CRYPTO32_NAME "libcrypto-${OVNR}_${ORNR}.dll")
      set(CRYPTO64_NAME "libcrypto-${OVNR}_${ORNR}-x64.dll")
      message(VERBOSE "CRYPTO32_NAME=${CRYPTO32_NAME}")
      message(VERBOSE "CRYPTO64_NAME=${CRYPTO64_NAME}")
      set(SSL32_NAME "libssl-${OVNR}_${ORNR}.dll")
      set(SSL64_NAME "libssl-${OVNR}_${ORNR}-x64.dll")
      message(VERBOSE "SSL32_NAME=${SSL32_NAME}")
      message(VERBOSE "SSL64_NAME=${SSL64_NAME}")

      get_filename_component(OPENSSL_EXECUTABLE_PATH ${OPENSSL_EXECUTABLE} DIRECTORY)
      message(VERBOSE "OPENSSL_EXECUTABLE_PATH=${OPENSSL_EXECUTABLE_PATH}")
      set(OPENSSL_EXECUTABLE_BIN_PATH "")
      string(REGEX MATCH "^(.*)/tools/openssl$" REGEX_MATCH "${OPENSSL_EXECUTABLE_PATH}")
      message(DEBUG "REGEX_MATCH=\"${REGEX_MATCH}\"")
      message(DEBUG "CMAKE_MATCH_1=\"${CMAKE_MATCH_1}\"")
      if (NOT ${REGEX_MATCH} EQUAL "")
        set(OPENSSL_EXECUTABLE_BIN_PATH "${CMAKE_MATCH_1}/bin") # bin path of this openssl variant
      endif()
      message(VERBOSE "OPENSSL_EXECUTABLE_BIN_PATH=${OPENSSL_EXECUTABLE_BIN_PATH}")

      unset(LIBCRYPTO_BIN)         # clear 
      unset(LIBCRYPTO_BIN CACHE)   # clear as well, because otherwise find_file might use it
      find_file(LIBCRYPTO_BIN
        NO_DEFAULT_PATH
        NAMES ${CRYPTO32_NAME} ${CRYPTO64_NAME}
        PATHS ${OPENSSL_EXECUTABLE_PATH} ${OPENSSL_EXECUTABLE_BIN_PATH}
      )
      message(VERBOSE "LIBCRYPTO_BIN=${LIBCRYPTO_BIN}")

      unset(LIBSSL_BIN)         # clear 
      unset(LIBSSL_BIN CACHE)   # clear as well, because otherwise find_file might use it
		  find_file(LIBSSL_BIN
        NO_DEFAULT_PATH
        NAMES ${SSL32_NAME} ${SSL64_NAME}
        PATHS ${OPENSSL_EXECUTABLE_PATH} ${OPENSSL_EXECUTABLE_BIN_PATH}
      )
      message(VERBOSE "LIBSSL_BIN=${LIBSSL_BIN}")

    else() # the version regex did not match
    
    # as a fallback, we check for "old" OpenSSL library (used before OpenSSL 1.1.0)

		find_file(LIBCRYPTO_BIN
			NAMES
			libeay32.dll
			HINTS
			${_OPENSSL_ROOT_HINTS}
			PATH_SUFFIXES
			bin)
		
		find_file(LIBSSL_BIN
			NAMES
			ssleay32.dll
			HINTS
			${_OPENSSL_ROOT_HINTS}
			PATH_SUFFIXES
			bin)
		
    endif()

		if(LIBCRYPTO_BIN AND LIBSSL_BIN)
 			set(OPENSSL_BIN_FOUND 1)
		endif()

	endif(WIN32 AND OPENSSL_VERSION)
		
endif(OPENSSL_FOUND)

