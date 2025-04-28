LUA_VERSION=5.4.4
BUILD_TYPE=debug
CC=cc
CXX=c++
SHELL=/bin/bash

all: build/$(BUILD_TYPE)
	cd build/$(BUILD_TYPE) && cd $$(pwd -P) && cmake .
	$(MAKE) -C build/$(BUILD_TYPE)

build/$(BUILD_TYPE):
	mkdir -p build/$(BUILD_TYPE)
	rev="$$(git rev-parse --short HEAD)" && current="$$(pwd -P)" && cd build/$(BUILD_TYPE) && cd "$$(pwd -P)" && cmake \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_VERBOSE_MAKEFILE=On \
		-DCMAKE_C_COMPILER="${CC}" \
		-DCMAKE_CXX_COMPILER="${CXX}" \
		-DCLINGO_BUILD_TESTS=On \
		-DCLASP_BUILD_TESTS=On \
		-DLIB_POTASSCO_BUILD_TESTS=On \
		-DCLINGO_BUILD_EXAMPLES=On \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=On \
		-DCMAKE_INSTALL_PREFIX=${HOME}/.local/opt/potassco/$(BUILD_TYPE) \
		-DCLINGO_BUILD_REVISION="$${rev}" \
		"$${current}"

# compdb can be installed with pip
compdb: build/$(BUILD_TYPE)
	compdb -p "build/$(BUILD_TYPE)" list -1 > compile_commands.json

stubs: all
	PYTHONPATH=build/$(BUILD_TYPE)/bin/python python scratch/mypy.py

%:: build/$(BUILD_TYPE) FORCE
	cd build/$(BUILD_TYPE) && cd $$(pwd -P) && cmake .
	$(MAKE) -C build/$(BUILD_TYPE) $@

test: build/$(BUILD_TYPE)
	cd build/$(BUILD_TYPE) && cd $$(pwd -P) && cmake .
	$(MAKE) -C build/$(BUILD_TYPE)
	$(MAKE) -C build/$(BUILD_TYPE) $@ CTEST_OUTPUT_ON_FAILURE=TRUE

gen: build/$(BUILD_TYPE)
	$(MAKE) -C build/$(BUILD_TYPE) gen
	mkdir -p libgringo/gen/src/
	rsync -ra --exclude clingopath.hh build/debug/libgringo/src/input libgringo/gen/src/

web: lua
	mkdir -p build/web
	rev="$$(git rev-parse --short HEAD)" && current="$$(pwd -P)" && cd build/web && cd "$$(pwd -P)" && source emsdk_env.sh && emcmake cmake \
		-DCLINGO_BUILD_WEB=On \
		-DCLINGO_BUILD_WITH_PYTHON=Off \
		-DCLINGO_BUILD_WITH_LUA=On \
		-DCLINGO_BUILD_SHARED=Off \
		-DCLASP_BUILD_WITH_THREADS=Off \
		-DCMAKE_VERBOSE_MAKEFILE=On \
		-DCMAKE_BUILD_TYPE=release \
		-DCMAKE_CXX_FLAGS="-std=c++11 -Wall -s DISABLE_EXCEPTION_CATCHING=0 -s ALLOW_MEMORY_GROWTH=1" \
		-DCMAKE_CXX_FLAGS_RELEASE="-Os -DNDEBUG" \
		-DCMAKE_EXE_LINKER_FLAGS="" \
		-DCMAKE_EXE_LINKER_FLAGS_RELEASE="" \
		-DLUA_LIBRARIES="$${current}/build/lua/install/lib/liblua.a" \
		-DLUA_INCLUDE_DIR="$${current}/build/lua/install/include" \
		-DCLINGO_BUILD_REVISION="$${rev}" \
		"$${current}"
	$(MAKE) -C build/web web

lua: build/lua/source/CMakeLists.txt
	source "emsdk_env.sh" && emcmake cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=build/lua/install -Hbuild/lua/source -Bbuild/lua/build
	$(MAKE) -C build/lua/build
	$(MAKE) -C build/lua/build install

build/lua/source/CMakeLists.txt:
	mkdir -p build/lua/source
	wget -O build/lua/lua.tar.gz https://www.lua.org/ftp/lua-$(LUA_VERSION).tar.gz
	tar xf build/lua/lua.tar.gz -C build/lua/source --strip-components 1
	cp scratch/lua/CMakeLists.txt build/lua/source/CMakeLists.txt

glob:
	find app libclingo libgringo libreify libluaclingo libpyclingo -name CMakeLists.txt | xargs ./cmake/glob-paths.py

FORCE:

.PHONY: all lua test web glob FORCE
