# Table of Contents

- [Requirements](#requirements)
  - [Development Dependencies](#development-dependencies)
  - [Optional Dependencies](#optional-dependencies)
- [Build, Install, and Test](#build-install-and-test)
  - [Build Options](#build-options)
    - [Generic Options](#generic-options)
    - [Python Support](#python-support)
    - [Lua Support](#lua-support)
  - [Compilation to JavaScript](#compilation-to-javascript)
- [Troubleshooting](#troubleshooting)
  - [Notes for Windows Users](#notes-for-windows-users)

# Requirements

This document is about installing clingo from source. We also provide
precompiled packages for various package managers:
<https://potassco.org/clingo/#packages>.

- a C++14 conforming compiler
  - *at least* [GCC](https://gcc.gnu.org/) version 4.9
  - [Clang](http://clang.llvm.org/) version 3.1 (using either libstdc++
    provided by gcc 4.9 or libc++)
  - *at least* MSVC 15.0 ([Visual Studio](https://www.visualstudio.com/) 2017)
  - other compilers might work
- the [cmake](https://www.cmake.org/) build system
  - at least version 3.18 is recommended
  - at least version 3.1 is *required*

## Development Dependencies

The following dependencies are only required when compiling a development
branch. Releases already include the necessary generated files.

- the [bison](https://www.gnu.org/software/bison/) parser generator
  - *at least* version 2.5
  - version 3.0 produces harmless warnings
    (to stay backwards-compatible)
- the [re2c](https://re2c.org/) lexer generator
  - *at least* version 1.1.1 is required

## Optional Dependencies

- the [Python](https://www.python.org/) script language and the
  [CFFI](https://cffi.readthedocs.io/) package
  - *at least* Python version 3.6
  - at least CFFI 1.14 is required (earlier versions have not been tested)
- the [Lua](https://www.lua.org/) script language
  - *at least* Lua 5.1 is required

# Build, Install, and Test

When cloning the git repository, do not forget to update the submodules (with
source releases, you can skip this step):

    git submodule update --init --recursive

To build gringo, clingo, and reify in their default configurations in release
mode, run:

    cmake -H<SOURCE_DIR> -B<BUILD_DIR> -DCMAKE_BUILD_TYPE=Release
    cmake --build <BUILD_DIR>

The resulting binaries and shared libraries will be in `<BUILD_DIR>/bin` and
are ready to use.

To install all binaries and development files under cmake's install
prefix (see the [build options](#build-options)), run:

    cmake --build <BUILD_DIR> --target install

To run the tests, enable option `CLINGO_BUILD_TESTS` (see [build
options](#build-options)) and run:

    cmake --build <BUILD_DIR> --target test

## Build Options

Cmake's `-L` option can be used to get an overview over the variables that can
be set for building gringo/clingo. To get gringo/clingo specific options, run

    cmake -H<SOURCE_DIR> -B<BUILD_DIR> -DCMAKE_BUILD_TYPE=Release -LH

or, to also print important cmake specific configuration variables

    cmake -H<SOURCE_DIR> -B<BUILD_DIR> -DCMAKE_BUILD_TYPE=Release -LAH

Options and variables can be passed to
cmake on the command line using `-D<VARIABLE>=<VALUE>` or by editing
`<BUILD_DIR>/CMakeCache.txt` after running cmake.

The build scripts by default try to detect optional dependencies, like Python
and Lua scripting support.

Clingo uses [libpotassco](https://github.com/potassco/libpotassco) and
[clasp](https://github.com/potassco/potassco).  Both components have their own
sets of configuration variables:
- [building libpotassco](https://github.com/potassco/libpotassco#installation)
- [building clasp](https://github.com/potassco/clasp#building--installing)

In the following, the most important options to control the build are listed.

### Generic Options

- Variable `CMAKE_BUILD_TYPE` should be set to `Release`.
- Variable `CMAKE_INSTALL_PREFIX` controls where to install clingo.
- Option `CLINGO_MANAGE_RPATH` controls how to find libraries on platforms
  where this is supported, like Linux, macOS, or BSD but not Windows. This
  option should be enabled if clingo is installed in a non-default location,
  like the users home directory; otherwise it has no effect.
  (Default: `ON`)
- Option `CLINGO_BUILD_APPS` controls whether to build the applications gringo,
  clingo, and reify.
  (Default: `ON`)
- Option `CLINGO_BUILD_EXAMPLES` controls whether to build the clingo API
  examples.
  (Default: `OFF`)
- Option `CLINGO_BUILD_TESTS` controls whether to build the clingo tests and
  enable the test target running unit as well as acceptance tests.
  (Default: `OFF`)

### Python Support

With the default configuration, Python support will be auto-detected if the
Python development packages are installed.

- Varibale `CLINGO_BUILD_WITH_PYTHON` can be set to `ON` to enable Python
  support, `OFF` to disable Python support, `auto` to enable Python support if
  available, or `pip` for advanced configuration to build a Python module
  exporting clingo symbols.
  (Default: `auto`)
- Variable `CLINGO_PYTHON_VERSION` can be used to select a specific Python
  version. For example, passing `-DCLINGO_PYTHON_VERSION:LIST="3.8;EXACT"` to
  cmake requires Python version 3.8 to be available and not just a compatbile
  Python version. Starting with cmake 3.15, variable `Python_ROOT` can be used
  to specify where to search for a Python installation (the documentation of
  cmake's `FindPython` module has further information).
  (Default: `3.6`)
- Variable `PYCLINGO_INSTALL` controls where to install the Python module. It
  can be set to `user` to install in the user prefix, `system` to install in
  the system prefix, or `prefix` to install into the installation prefix.
  (Default: `prefix`)
- Variable `PYCLINGO_SUFFIX` can be used to customize which suffix to use for
  the Python module.
  (Default: automatically detected)
- Variable `PYCLINGO_INSTALL_DIR` can be used to customize where to install the
  Python module.
  (Default: automatically detected)

With cmake versions before 3.15, it can happen that the found Python
interpreter does not match the found Python libraries if the development
headers for the interpreter are not installed. Make sure to install them before
running cmake (or remove or adjust the `CMakeCache.txt` file).

### Lua Support

With the default configuration, Lua support will be auto-detected if the Lua
development packages are installed.

- Varibale `CLINGO_BUILD_WITH_LUA` can be set to `ON` to enable Lua support,
  `OFF` to disable Lua support, `auto` to enable Lua support if available.
  (Default: `auto`)
- Variable `CLINGO_LUA_VERSION` can be used to select a specific Lua
  version. For example, passing `-DCLINGO_LUA_VERSION:LIST="5.3;EXACT"` to
  cmake requires Lua version 5.3 to be available and not just a compatbile
  Lua version.
  (Default: `5.0`)
- Variable `LUACLINGO_SUFFIX` can be used to customize which suffix to use for
  the Lua module.
  (Default: automatically detected)
- Variable `LUACLINGO_INSTALL_DIR` can be used to customize where to install
  the Lua module.
  (Default: automatically detected)

## Compilation to JavaScript

Clingo can be compiled to JavaScript with Empscripten. The following notes
assume that [Emscripten](https://kripken.github.io/emscripten-site/) has been
installed. Only the web target and a subset of clingo's configuration are
supported when compiling to JavaScript:

    emcmake cmake -H<SOURCE_DIR> -B<BUILD_DIR> \
        -DCLINGO_BUILD_WEB=On \
        -DCLINGO_BUILD_WITH_PYTHON=Off \
        -DCLINGO_BUILD_WITH_LUA=Off \
        -DCLINGO_BUILD_SHARED=Off \
        -DCLASP_BUILD_WITH_THREADS=Off \
        -DCMAKE_VERBOSE_MAKEFILE=On \
        -DCMAKE_BUILD_TYPE=release \
        -DCMAKE_CXX_FLAGS="-std=c++11 -Wall -s DISABLE_EXCEPTION_CATCHING=0" \
        -DCMAKE_CXX_FLAGS_RELEASE="-Os -DNDEBUG" \
        -DCMAKE_EXE_LINKER_FLAGS="" \
        -DCMAKE_EXE_LINKER_FLAGS_RELEASE=""
    cmake --build <BUILD_DIR> --target web

Note that is is possible to enable Lua support. Therefore Lua has to be
compiled with emscripten, too. See [Lua Support](#lua-support) for information
about pointing clingo to a custom Lua installation.

For examples how to use the resulting JavaScript code, check out one of the
following:
- [webclingo example by Lucas Bourneuf](https://github.com/Aluriak/webclingo-example), or
- [the source of our website](https://github.com/potassco/potassco.github.io)

# Troubleshooting

After installing the required packages clingo should compile on most \*nixes.
If a dependency is missing or a software version too old, then there are
typically community repositories that provide the necessary packages. To list a
few:
- the [ToolChain](https://wiki.ubuntu.com/ToolChain) repository for Ubuntu
  versions before 18.04.
- the [Developer
  Toolset](https://wiki.centos.org/SpecialInterestGroup/SCLo/CollectionsList)
  for CentOS
- the [Cygwin](http://cygwin.org) project under Windows (re2c must be compiled
  by hand)
- both [Homebrew](https://brew.sh/) and [MacPorts](https://www.macports.org/)
  provide all the software necessary to compile clingo

And, well, you can compile a recent gcc version yourself. Even on ancient Linux
systems. ;)

## Notes for Windows Users

clingo can be compiled using the
[Mingw-w64](https://mingw-w64.sourceforge.net/) compiler, the Cygwin project,
or Visual Studio 2015 Update 3. For development, The [re2c](https://re2c.org/)
and [winflexbison](https://github.com/lexxmark/winflexbison) (providing
[bison](https://www.gnu.org/software/bison/) for Windows) packages can be
installed using the [chocolatey](https://chocolatey.org/) package manager, too.
