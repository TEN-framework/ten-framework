project(lws-crypto-cose-key C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-crypto-cose-key)
set(SRCS main.c)

set(requirements 1)
require_lws_config(LWS_WITH_COSE 1 requirements)

if (requirements)

	add_executable(${SAMP} ${SRCS})
	
	add_test(NAME crypto-cose-key-1
		 COMMAND lws-crypto-cose-key --stdin set1.cks )
	add_test(NAME crypto-cose-key-2
		 COMMAND lws-crypto-cose-key --kty EC2 --curve P-256 --kid ctest-256 --stdout ctest-ec-256.key)
	add_test(NAME crypto-cose-key-3
		 COMMAND lws-crypto-cose-key --kty EC2 --curve P-384 --kid ctest-384 --stdout ctest-ec-384.key)
	add_test(NAME crypto-cose-key-4
		 COMMAND lws-crypto-cose-key --kty EC2 --curve P-521 --kid ctest-512 --stdout ctest-ec-512.key)
	add_test(NAME crypto-cose-key-5
		 COMMAND lws-crypto-cose-key --kty SYMMETRIC --bits 256 --stdout ctest-sym-256.key)
	add_test(NAME crypto-cose-key-6
		 COMMAND lws-crypto-cose-key --kty RSA --bits 2048 --stdout ctest-rsa-2048.key)
	add_test(NAME crypto-cose-key-7
		 COMMAND lws-crypto-cose-key --stdin ctest-rsa-2048.key)

	set_tests_properties(crypto-cose-key-1
			     crypto-cose-key-2
			     crypto-cose-key-3
			     crypto-cose-key-4
			     crypto-cose-key-5
			     crypto-cose-key-6
			     crypto-cose-key-7
			     PROPERTIES
		     		WORKING_DIRECTORY
		     			${CMAKE_SOURCE_DIR}/minimal-examples/crypto/minimal-crypto-cose-key
		     		TIMEOUT 5)
		     		
	set_tests_properties(crypto-cose-key-7
			     PROPERTIES
		     		DEPENDS crypto-cose-key-6)

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
