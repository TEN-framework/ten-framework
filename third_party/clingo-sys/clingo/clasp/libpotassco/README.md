# libpotassco

[![Build Status master](https://badges.herokuapp.com/travis/potassco/libpotassco?branch=master&label=master)](https://travis-ci.org/potassco/libpotassco?branch=master)

libpotassco is a small C++ utility library used by various potassco projects
that mostly provides functions and types for
 - parsing, writing, and converting logic programs in [aspif][aspif] and smodels format,
 - passing information between a grounder and a solver,
 - defining and parsing command-line options and for creating command-line applications.

Furthermore, it comes with the tool `lpconvert` that converts either between aspif and smodels format
or to a human-readable text format.

libpotassco is part of the potassco project. For further information please visit:

  http://potassco.org/

## Installation

The preferred way to build libpotassco is to use [CMake][cmake] 
version 3.1 or later.

The following options can be used to configure the build:
  
    LIB_POTASSCO_BUILD_APP  : whether or not to build the lpconvert tool
    LIB_POTASSCO_BUILD_TESTS: whether or not to build unit tests

For example, to build libpotassco in release mode in directory `<dir>`:

    cmake -H. -B<dir>
    cmake --build <dir>

The following options can be used to configure the installation:
    
    CMAKE_INSTALL_PREFIX    : install path prefix
    LIB_POTASSCO_INSTALL_LIB: whether or not to install libpotassco

For example, to install lpconvert and libpotassco under `/home/<usr>`:

    cmake -H. -B<dir> -DCMAKE_INSTALL_PREFIX=/home/<usr> -DLIB_POTASSCO_INSTALL_LIB=ON
    cmake --build <dir> --target install

To use libpotassco in a cmake-based project either:

- Place the library inside your project, e.g. using [git submodules](http://git-scm.com/docs/git-submodule).
- Call `add_subdirectory(<path_to_libpotassco>)`.

or, if libpotassco is installed in `CMAKE_PREFIX_PATH`:
- Call `find_package(Potassco <major>.<minor> CONFIG)`.

Finally, call `target_link_libraries(your_target PUBLIC libpotassco)` to link to the potassco library.

## Documentation
Source code documentation can be generated with [Doxygen][doxygen].
Either explicitly:
  
    cd doc/
    doxygen

or via the `doc_potassco` target when using cmake.
  
[aspif]: https://www.cs.uni-potsdam.de/wv/publications/DBLP_conf/iclp/GebserKKOSW16x.pdf  "Aspif specification"
[cmake]: https://cmake.org/
[doxygen]: http://www.doxygen.nl/
