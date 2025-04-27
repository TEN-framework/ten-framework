# clasp

clasp is an answer set solver for (extended) normal and disjunctive logic programs.
It is part of the [Potassco](https://potassco.org) project for *Answer Set Programming* (ASP).
The primary algorithm of clasp relies on conflict-driven nogood learning,
a technique that proved very successful for satisfiability checking (SAT).
clasp has been genuinely developed for answer set solving but can 
also be applied as a (Max-)SAT or PB solver or as a C++ library in another program.
It provides different reasoning modes and other advanced features including:
 
 - [Enumeration][enum] and [Optimization][opt] of ([Projected][proj]) Solutions,
 - Cautious and Brave Reasoning,
 - [Advanced Disjunctive Solving][claspD2],
 - [Parallel (multithreaded) solving][claspmt],
 - [Domain heuristic][hclasp] modifications,
 - [Unsatisfiable-core based optimization][unclasp],
 - [ASP/SAT/PB modulo acyclicity][acyc],
 - Different input formats including [smodels][smodels], [aspif][aspif], [dimacs][dimacs] and [opb][opb].

Detailed information (including a User's manual), source code,
and pre-compiled binaries are available at: http://potassco.org/

## LICENSE
  clasp is distributed under the MIT License.
  
  See LICENSE for details regarding the license.

## PACKAGE CONTENTS
    LICENSE        - The MIT License
    CHANGES        - Major changes between versions
    README.md      - This file
    CMakeLists.txt - Configuration file for building clasp with CMake
    cmake/         - Module directory for additional CMake scripts
    app/           - Source code directory of the command-line interface
    clasp/         - Header directory of the clasp library
    src/           - Source code directory of the clasp library
    tests/         - Unit tests of the clasp library
    examples/      - Examples using the clasp library
    libpotassco/   - Directory of the potassco library
    tools/         - Some additional files
  

## BUILDING & INSTALLING
  The preferred way to build clasp is to use [CMake][cmake] version 3.1 or later
  together with a C++ compiler that supports C++11.

  The following options can be used to configure the build:
  
    CLASP_BUILD_APP         : whether or not to build the clasp application
    CLASP_BUILD_TESTS       : whether or not to build clasp unit tests
    CLASP_BUILD_EXAMPLES    : whether or not to build examples
    CLASP_BUILD_WITH_THREADS: whether or not to build clasp with threading support
                              (requires C++11)

  For example, to build clasp in release mode in directory `<dir>`:

    cmake -H. -B<dir>
    cmake --build <dir>

  To install clasp afterwards:
  
    cmake --build <dir> --target install

  To set the installation prefix, run
  `cmake` with option `-DCMAKE_INSTALL_PREFIX=<path>`.

  Finally, you can always skip installation and simply copy the
  clasp executable to a directory of your choice.

## DOCUMENTATION
  A User's Guide is available from http://potassco.org/
  
  Source code documentation can be generated with [Doxygen][doxygen].
  Either explicitly:
  
    cd libclasp/doc/api
    doxygen clasp.doxy

  or via the `doc_clasp` target when using cmake.

## USAGE
  clasp reads problem instances either from stdin, e.g
  
    cat problem | clasp
  
  or from a given file, e.g
  
    clasp problem

  Type
  
    clasp --help
  
  to get a basic overview of options supported by clasp or
  
    clasp --help={2,3}
  
  for a more detailed list.

  In addition to printing status information, clasp also
  provides information about the computation via its exit status.
  The exit status is either one or a combination of:
    
    0  : search was not started because of some option (e.g. '--help')
    1  : search was interrupted
    10 : problem was found to be satisfiable
    20 : problem was proved to be unsatisfiable
  
  Exit codes 1 and 11 indicate that search was interrupted before
  the final result was computed. Exit code 30 indicates that either
  all models were found (enumeration), optimality was proved (optimization),
  or all consequences were computed (cautious/brave reasoning).
  Finally, exit codes greater than 32 are used to signal errors.

[enum]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:conf/lpnmr/GebserKNS07
[proj]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:conf/cpaior/GebserKS09
[opt]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:journals/tplp/GebserKS11
[claspmt]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:journals/tplp/GebserKS12
[claspD2]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:conf/ijcai/GebserKS13
[hclasp]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:conf/aaai/GebserKROSW13
[unclasp]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:conf/iclp/AndresKMS12
[acyc]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:journals/fuin/BomansonGJKS16
[aspif]: https://www.cs.uni-potsdam.de/wv/publications/#DBLP:conf/iclp/GebserKKOSW16x
[smodels]: http://www.tcs.hut.fi/Software/smodels/lparse.ps
[dimacs]: http://www.satcompetition.org/2009/format-benchmarks2009.html
[opb]: https://www.cril.univ-artois.fr/PB09/solver_req.html
[doxygen]: https://www.stack.nl/~dimitri/doxygen/
[cmake]: https://cmake.org/
