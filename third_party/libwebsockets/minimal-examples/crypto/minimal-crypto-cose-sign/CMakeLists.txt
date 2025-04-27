project(lws-crypto-cose-sign C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-crypto-cose-sign)
set(SRCS main.c)

set(requirements 1)
require_lws_config(LWS_WITH_COSE 1 requirements)

if (requirements)

	add_executable(${SAMP} ${SRCS})

	# EC signing
	
	add_test(NAME crypto-cose-sign-1
		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid 11
		 	--alg ES256 --cose-sign
		 	--stdin payload.txt
		 	--stdout ctest-sig-es256.sig)
	add_test(NAME crypto-cose-sign-2
		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid sec384
		 	--alg ES384 --cose-sign
		 	--stdin payload.txt
		 	--stdout ctest-sig-es384.sig)
	add_test(NAME crypto-cose-sign-3
		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid sec512
		 	--alg ES512 --cose-sign
		 	--stdin payload.txt
		 	--stdout ctest-sig-es512.sig)

	# EC validation
		 		
	add_test(NAME crypto-cose-sign-4
		 COMMAND lws-crypto-cose-sign -k set1.cks
		 	--stdout r1.txt
		 	--stdin ctest-sig-es256.sig)
	set_tests_properties(crypto-cose-sign-4	PROPERTIES
						DEPENDS crypto-cose-sign-1)
		 	
	add_test(NAME crypto-cose-sign-5
		 COMMAND lws-crypto-cose-sign -k set1.cks
		 	--stdout r2.txt
		 	--stdin ctest-sig-es384.sig)
	set_tests_properties(crypto-cose-sign-5	PROPERTIES
						DEPENDS crypto-cose-sign-2)
		 	
	add_test(NAME crypto-cose-sign-6
		 COMMAND lws-crypto-cose-sign -k set1.cks --cose-sign
		 	--stdout r3.txt
		 	--stdin ctest-sig-es512.sig)
	set_tests_properties(crypto-cose-sign-6	PROPERTIES
						DEPENDS crypto-cose-sign-3)

	# RSA 4096 signing
	
	add_test(NAME crypto-cose-sign-7
	 COMMAND lws-crypto-cose-sign -s -k rsa-4096.ck
	 	--alg RS512 --cose-sign
	 	--stdin payload.txt
	 	--stdout ctest-sig-rs512.sig)
	 	
	 # RSA 4096 validation

	add_test(NAME crypto-cose-sign-8
		 COMMAND lws-crypto-cose-sign -k rsa-4096.ck --cose-sign
		 	--stdout r8.txt
		 	--stdin ctest-sig-rs512.sig)
	set_tests_properties(crypto-cose-sign-8	PROPERTIES
						DEPENDS crypto-cose-sign-7)
						
	# HMAC signing, cose-mac

#	add_test(NAME crypto-cose-sign-9
#		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid our-secret
#		 	--alg HS256 --cose-mac
#		 	--stdin payload.txt
#		 	--stdout ctest-sig-hmac256.sig)
#	add_test(NAME crypto-cose-sign-10
#		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid sec-48
#		 	--alg HS384 --cose-mac
#		 	--stdin payload.txt
#		 	--stdout ctest-sig-hmac384.sig)
#	add_test(NAME crypto-cose-sign-11
#		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid sec-64
#		 	--alg HS512 --cose-mac
#		 	--stdin payload.txt
#		 	--stdout ctest-sig-hmac512.sig)
#	add_test(NAME crypto-cose-sign-12
#		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid our-secret
#		 	--alg HS256_64 --cose-mac
#		 	--stdin payload.txt
#		 	--stdout ctest-sig-hmac256_64.sig)

	# HMAC validation, cose-mac
	
#	add_test(NAME crypto-cose-sign-13
#		 COMMAND lws-crypto-cose-sign -k set1.cks
#		 	--stdout r1.txt
#		 	--stdin ctest-sig-hmac256.sig)
#	set_tests_properties(crypto-cose-sign-13
#			     PROPERTIES DEPENDS crypto-cose-sign-9)
#		 	
#	add_test(NAME crypto-cose-sign-14
#		 COMMAND lws-crypto-cose-sign -k set1.cks
#		 	--stdout r2.txt
#		 	--stdin ctest-sig-hmac384.sig)
#	set_tests_properties(crypto-cose-sign-14
#			     PROPERTIES DEPENDS crypto-cose-sign-10)
#		 	
#	add_test(NAME crypto-cose-sign-15
#		 COMMAND lws-crypto-cose-sign -k set1.cks
#		 	--stdout r3.txt
#		 	--stdin ctest-sig-hmac512.sig)
#	set_tests_properties(crypto-cose-sign-15
#			     PROPERTIES DEPENDS crypto-cose-sign-11)
#			     
#	add_test(NAME crypto-cose-sign-16
#		 COMMAND lws-crypto-cose-sign -k set1.cks
#		 	--stdout r4.txt
#		 	--stdin ctest-sig-hmac256_64.sig)
#	set_tests_properties(crypto-cose-sign-16
#			     PROPERTIES DEPENDS crypto-cose-sign-12)
			     
	# HMAC signing, cose-mac0

	add_test(NAME crypto-cose-sign-17
		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid our-secret
		 	--alg HS256 --cose-mac0
		 	--stdin payload.txt
		 	--stdout ctest-sig-hmac0256.sig)
	add_test(NAME crypto-cose-sign-18
		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid sec-48
		 	--alg HS384 --cose-mac0
		 	--stdin payload.txt
		 	--stdout ctest-sig-hmac0384.sig)
	add_test(NAME crypto-cose-sign-19
		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid sec-64
		 	--alg HS512 --cose-mac0
		 	--stdin payload.txt
		 	--stdout ctest-sig-hmac0512.sig)
	add_test(NAME crypto-cose-sign-20
		 COMMAND lws-crypto-cose-sign -s -k set1.cks --kid our-secret
		 	--alg HS256_64 --cose-mac0
		 	--stdin payload.txt
		 	--stdout ctest-sig-hmac0256_64.sig)

	# HMAC validation, cose-mac0
	
	add_test(NAME crypto-cose-sign-21
		 COMMAND lws-crypto-cose-sign -k set1.cks
		 	--stdout r1.txt
		 	--stdin ctest-sig-hmac0256.sig)
	set_tests_properties(crypto-cose-sign-21
			     PROPERTIES DEPENDS crypto-cose-sign-17)
		 	
	add_test(NAME crypto-cose-sign-22
		 COMMAND lws-crypto-cose-sign -k set1.cks
		 	--stdout r2.txt
		 	--stdin ctest-sig-hmac0384.sig)
	set_tests_properties(crypto-cose-sign-22
			     PROPERTIES DEPENDS crypto-cose-sign-18)
		 	
	add_test(NAME crypto-cose-sign-23
		 COMMAND lws-crypto-cose-sign -k set1.cks
		 	--stdout r3.txt
		 	--stdin ctest-sig-hmac0512.sig)
	set_tests_properties(crypto-cose-sign-23
			     PROPERTIES DEPENDS crypto-cose-sign-19)
			     
	add_test(NAME crypto-cose-sign-24
		 COMMAND lws-crypto-cose-sign -k set1.cks
		 	--stdout r4.txt
		 	--stdin ctest-sig-hmac0256_64.sig)
	set_tests_properties(crypto-cose-sign-24
			     PROPERTIES DEPENDS crypto-cose-sign-20)


	set_tests_properties(crypto-cose-sign-1
			     crypto-cose-sign-2
			     crypto-cose-sign-3
			     crypto-cose-sign-4
     			     crypto-cose-sign-5
			     crypto-cose-sign-6
			     crypto-cose-sign-7
			     crypto-cose-sign-8
#    			     crypto-cose-sign-9
#       		     crypto-cose-sign-10
#			     crypto-cose-sign-11
#			     crypto-cose-sign-12
#			     crypto-cose-sign-13
#			     crypto-cose-sign-14
#			     crypto-cose-sign-15
#			     crypto-cose-sign-16
			     crypto-cose-sign-17
			     crypto-cose-sign-18
			     crypto-cose-sign-19
			     crypto-cose-sign-20
			     crypto-cose-sign-21
			     crypto-cose-sign-22
			     crypto-cose-sign-23
			     crypto-cose-sign-24

			     PROPERTIES
		     		WORKING_DIRECTORY
		     			${CMAKE_SOURCE_DIR}/minimal-examples/crypto/minimal-crypto-cose-sign
		     		TIMEOUT 5)

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
