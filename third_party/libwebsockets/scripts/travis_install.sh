#!/bin/bash

if [ "$COVERITY_SCAN_BRANCH" == 1 ]; then exit; fi

if [ "$TRAVIS_OS_NAME" == "linux" ];
then
	sudo apt-get update -qq

	if [ "$LWS_METHOD" == "lwsws" -o "$LWS_METHOD" == "lwsws2" ];
	then
		sudo apt-get install -y -qq realpath libjemalloc1 libev4 libuv-dev libdbus-1-dev valgrind mosquitto
		sudo apt-get remove python-six
		sudo pip install "six>=1.9"
		sudo pip install "Twisted==16.0.0"
		sudo pip install "pyopenssl>=0.14"
		sudo pip install autobahntestsuite
		wget https://libwebsockets.org/openssl-1.1.0-trusty.tar.bz2 -O/tmp/openssl.tar.bz2
		cd /
		sudo tar xf /tmp/openssl.tar.bz2
		sudo ldconfig
		sudo update-ca-certificates
	fi

	if [ "$LWS_METHOD" == "mbedtls" -o "$LWS_METHOD" == "ss+mbedtls" ];
	then
		sudo apt-get install -y -qq realpath libjemalloc1 libev4 libuv-dev valgrind
		wget https://libwebsockets.org/openssl-1.1.0-trusty.tar.bz2 -O/tmp/openssl.tar.bz2
		cd /
		sudo tar xf /tmp/openssl.tar.bz2
		sudo ldconfig
		sudo update-ca-certificates
	fi

	if [ "$LWS_METHOD" == "smp" ];
	then
		sudo apt-get install -y -qq realpath libjemalloc1 libev4
		wget https://libwebsockets.org/openssl-1.1.0-trusty.tar.bz2 -O/tmp/openssl.tar.bz2
		cd /
		sudo tar xf /tmp/openssl.tar.bz2
		sudo ldconfig
		sudo update-ca-certificates
	fi

	if [ "$LWS_METHOD" == "libev" ];
	then
		sudo apt-get install -y -qq libev-dev;
	fi

	if [ "$LWS_METHOD" == "libuv" -o "$LWS_METHOD" == "lwsws" -o "$LWS_METHOD" == "lwsws2" ];
	then
		sudo apt-get install -y -qq libuv-dev;
#libuv1 libuv1-dev;
	fi

fi

if [ "$TRAVIS_OS_NAME" == "osx" ];
then
	if [ "$LWS_METHOD" == "lwsws" -o "$LWS_METHOD" == "lwsws2" ];
	then
		brew update;
		brew install dbus;
	fi

	if [ "$LWS_METHOD" == "libev" ];
	then
		brew update;
		brew install libev;
	fi

	if [ "$LWS_METHOD" == "libuv" -o "$LWS_METHOD" == "lwsws" -o "$LWS_METHOD" == "lwsws2" ];
	then
		brew update;
		brew install libuv;
	fi

fi

	
