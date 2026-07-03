#!/bin/bash

if [ "$COVERITY_SCAN_BRANCH" != 1 -a "$TRAVIS_OS_NAME" = "osx" ]; then
	if [ "$LWS_METHOD" != "mbedtls" -a "$LWS_METHOD" != "ss+mbedtls" ] ; then
		mkdir build && cd build &&
		cmake -DOPENSSL_ROOT_DIR="/usr/local/opt/openssl" $CMAKE_ARGS .. &&
		cmake --build .
	fi
else
	if [ "$COVERITY_SCAN_BRANCH" != 1 -a "$TRAVIS_OS_NAME" = "linux" ]; then
		mkdir build && cd build &&
		if [ "$LWS_METHOD" = "lwsws" ] ; then
			cmake -DLWS_OPENSSL_LIBRARIES="/usr/local/lib/libssl.so;/usr/local/lib/libcrypto.so" \
			      -DLWS_OPENSSL_INCLUDE_DIRS="/usr/local/include/openssl" $CMAKE_ARGS .. &&
			cmake --build . &&
			sudo make install &&
			../minimal-examples/selftests.sh &&
			../scripts/h2spec.sh &&
			../scripts/attack.sh &&
			../scripts/h2load.sh &&
			../scripts/autobahn-test-server.sh &&
			../scripts/autobahn-test-client.sh
		else
			if [ "$LWS_METHOD" = "lwsws2" ] ; then
				cmake -DLWS_OPENSSL_LIBRARIES="/usr/local/lib/libssl.so;/usr/local/lib/libcrypto.so" \
				      -DLWS_OPENSSL_INCLUDE_DIRS="/usr/local/include/openssl" $CMAKE_ARGS .. &&
				cmake --build . &&
				sudo make install &&
				../scripts/autobahn-test-server.sh
			else
				if [ "$LWS_METHOD" = "smp" ] ; then
					cmake -DLWS_OPENSSL_LIBRARIES="/usr/local/lib/libssl.so;/usr/local/lib/libcrypto.so" \
					      -DLWS_OPENSSL_INCLUDE_DIRS="/usr/local/include/openssl" $CMAKE_ARGS .. &&
					cmake --build . &&
					../scripts/h2load-smp.sh
				else
					if [ "$LWS_METHOD" = "mbedtls" -o "$LWS_METHOD" = "ss+mbedtls" ] ; then
						cmake $CMAKE_ARGS .. &&
						cmake --build . &&
						sudo make install &&
						../minimal-examples/selftests.sh &&
						../scripts/h2spec.sh &&
						../scripts/h2load.sh &&
						../scripts/attack.sh
					else
						cmake $CMAKE_ARGS .. &&
						cmake --build .
					fi
				fi
			fi
		fi
	fi
fi

