# Changes

## clingo 5.7.1

  * fix comparison of theory elements (#485)
  * fix manifest (#484)
  * update cibw to also produce Python 3.12 wheels

## clingo 5.7.0

  * add AST node for comments (#417)
  * add function to change undo mode (#409)
  * add function to access priorities to API (#406)
  * add `Model::is_consequence` to API (#423)
  * add option to preserve facts (#457)
  * improve hash table performance (#441)
  * extend add_theory_atom method of backend (#461)
    (breaks backward compatibility of the API)
  * add contribution guidelines (#465)
  * fix `add_theory_atom_with_guard` in Python API
  * fix AST bugs (#403)
  * fix parsing of hexadecimal numbers (#421)
  * fix assignment aggregates (#436)
  * fix build scripts for Python 3.12 (#452)
  * fix overflows in IESolver (#462)
  * make sure `clingo_control_ground` is re-entrant (#418)
  * update clasp and dependencies

## clingo 5.6.2

  * fix AST comparison (#394)
  * fix handling of n-ary comparisons in AST (#396)

## clingo 5.6.1

  * fix symbolic atom iterator (#389)
  * build wheels using cibuildwheel (#391, #392)
    (this adds additional architectures)

## clingo 5.6.0

  * add support for parsing files in ASPIF format (#387)
    (breaks C API)
  * add theory related functions to backend (#381)
  * add support for comparisons with more than one relation (#368)
  * extend safety by computing intervals from comparisons (#375)
  * add and ground base part by default in Python API (#378)
  * remove experimental CSP extensions (#367)
  * require at least MSVC 15.0 to build on Windows
  * large code refactoring (#376)
  * replace internal hash set implementation by external one (#386)
  * update to clasp version 3.3.9

## clingo 5.5.2

  * fix `parse_files` in C++ API
  * fix adding clauses during enumeration (#359)
  * fix incremental projection with backend (#362)
  * update to clasp version 3.3.8

## clingo 5.5.1

  * extend theory class to get version information
  * improve performance of `Model.symbol` (#296)
  * tidy up `clingo.hh` header regarding C++17 deprecations (#344)
  * fix error handling while solving in Python API (#334)
  * fix various outher bugs
  * update to clasp version 3.3.7

## clingo 5.5.0

  * allow for using `not` as a theory operator (#193)
  * improve parsing of disjunctions (#171)
  * rename `max_size` to `size` in APIs and remove previous `size` method
    (breaks backward compatibility but makes things consistent)
  * refine cmake configuration variables (#283)
    (package maintainers might have to pay attention here)
  * extend clingo API (#236, #231, #228, #227, #187, #171, #174, #183)
  * add type annotations and add stub files for type checkers (#212)
  * reimplement Python API using CFFI (#253)
    (breaks backward compatibility regarding the AST and some details)
  * add `remove_watch` and `freeze_literal` to `propagate_init` (#285)
  * add support for Lua 5.4
  * add options to select specific Lua/Python versions
  * add single-shot solving option (#269)
    (see installation instructions)
  * rename `clingo.Tuple` to `clingo.Tuple_`
    (to avoid name clashes with `typing.Tuple`)
  * fix propagator initialization (#166)
  * fix cleanup function and handling of theory (#169)
  * fix shifting of disjunctions (in clasp) (#173)
  * fix handling of pools in externals (#179)
  * fix logger in Python API (#175)
  * fix memory bugs regarding strings (#196)
  * fix undo function (#191)
  * fix adding literals in propagate init (#192)
  * fix handling of undefined arithmetics (#218)
  * fix incremental grounding (#248)
  * fix/improve handling of classical negation (#268)
  * update to clasp version 3.3.6 fixing various issues

## clingo 5.4.0

  * add extension to implement custom heuristics
  * add const modifiers to C API
  * add flags to external and const statements to match API functions
  * fix python memleaks
  * make compatible with msvc 15
  * C ABI changes
    * extended propagators
  * C++ API changes
    * unify usage of ProgramBuilder and Backend
  * python API changes
    * `TruthValue.{_True,_False}` becomes `TruthValue.{True_,False_}`
    * `HeuristicType.{True,False}` becomes `TruthValue.{True_,False_}`
    * `async` and yield becomes `async_` and `yield_`
  * improve python API documentation
  * use cmakes `find_python` module
  * update to clasp version 3.3.5

## clingo 5.3.0

  * change C API to use numeric instead of symbolic literals
    * affects assumptions and assigning/releasing externals
      (breaks backward compatibility)
    * added overloads to C++, python and lua API to support
      both numeric and symbolic version
      (preserves backward compatibility for most code)
  * the python, C and C++ APIs now allow for customizing clingo by implementing
    a custom main function but reusing the rest of the application including
    the standard output
  * add API function to detect conflicting programs
  * add message logger to python and lua interface
  * add support for primes in the beginning of identifiers and variable names
  * add per solver registration of watches during propagator initialization
  * add a directive to selectivel suppress undefined atom warnings
  * add support for user defined statistics
  * add _to_c functions for python API to be able to call C functions from
    python
  * only create ground representations for requested program parts when
    grounding (#71)
  * improve program observer (#19)
  * support for binary, octal, and hexadecimal numbers (#65)
  * the backend has to be opened/closed now
  * release python's GIL while grounding (#82)
  * TruthValue.{True,False} becomes TruthValue.{\_True,\_False} in python API
  * improve API and it's documentation

## clingo 5.2.3

  * update to clasp version 3.3.4
  * really fix --pre option
  * fix link to potassco guide (#74)
  * fix location printing (#78)
  * fix linking problems (#79)
  * fix modulo zero (#100)
  * fix web builds (#103)
  * fix addding clauses after a model has been found (#104)
  * fix python program observer bindings (#105)
  * expose exponentiation operator in APIs
  * improve python docstrings (#101, #102)
  * add option to build python and lua modules against an existing libclingo

## clingo 5.2.2

  * update to clasp version 3.3.3
  * use GNUInstallDirs in cmake files to simplify packaging
  * fix --pre option
  * fix swapped clingo\_assignment\_size and clingo\_assignment\_max\_size
  * fix docstrings
  * fix incremental mode
  * fix sup and inf in python/lua bindings
  * fix reified format term tuples
  * fix wrong use of python API (causing trouble with python 3.6)
  * fix compilation problems on 32bit linux (missing libatomic)

## clingo 5.2.1

  * update to clasp version 3.3.2
  * fix handling of istop in incmode programs
  * fix handling of undefined ** operations
  * fix preprocessing of disjunctions with undefined operations
    (regression in clingo-5)
  * fix segfault during preprocessing
    (regression in clingo-5)

## clingo 5.2.0

  * switch to MIT license
  * improve compatibility with abstract gringo
  * switch build system from scons to cmake
  * improve windows compatibility
  * make tests and examples python 3 compatible
  * bison and re2c are no longer required to build source releases
  * update to clasp 3.3.0
  * the CLINGOPATH environment variable can be set
    to control from where to include files in logic programs
  * propagators can add variables while solving now
  * refactor interfaces (breaking backward compatibility)
    * there is just one solve function now
    * in the C API do not pass structs by value to functions
      because FFIs of some languages do not support this
  * fix cleanup function
  * numerous other bugfixes not listed here

## clingo 5.1.1

  * fix thread id to start with one in propagator.undo in lua
  * fix version macro in clingo.h
  * fix added missing methods to get thread id to model in lua/python
  * fix child\_key property in python ast

## clingo 5.1.0

  * update to clasp 3.2.1
  * add interface to add variables during propagation
  * add interface to inspect ground rules (C/C++ only)
  * add experimental interface to access clasp facade (C/C++ only)
  * fixed smodels output (--output=smodels)

## clingo 5.0.0

  * cleanup of python and lua API (breaks backwards compatibility)
  * added new aspif output format replacing the old smodels format
  * added input language support for clasp features
    * #edge directives to add acyclicity constraints
    * #project directives for enumeration of projected models
    * #heuristic directives to steer clasp's search
  * added theory atoms to write aggregate like constructs
  * added stable C API documented with doxygen
  * added experimental C++ API based on C API
  * added theory propagator interface to clingo APIs
  * added support for compilation with Visual Studio 2015
  * improved data structures to reduce memory consumption on typical input
  * updated to clasp version 3.2.0 + patches

## gringo/clingo 4.5.4

  * fixed bug when creating multiple Control objects
    (affects lua only)
  * fixed bug when trying to configure more solvers than in portfolio
    (affects python only)
  * fixed #disjoint constraints
  * improved build scripts
  * added option to keep facts in normal rules

## gringo/clingo 4.5.3

  * fixed regression w.r.t gringo 4.4 in translation of conditional literals
  * fixed projection in incremental programs
  * fixed bug with (double) negative literals in minimize constraints

## gringo/clingo 4.5.2

  * fixed memory leak in python API when enumerating models
  * updated to clasp version 3.1.3

## gringo/clingo 4.5.1

  * ground term parser returns None/nil for undefined terms now
  * added warning if a global variable occurs in a tuple of an aggregate element
  * added auto detection of libraries
  * changed option --update-domains into API function Control:cleanup\_domains
  * fixed domain cleanup when used with minimize constraints
  * fixed grounding of recursive disjunctions (regression in 4.5.0)
  * fixed Control.stats in lua bindings
  * fixed a bug in clingo that would print 0-ary classically negated atoms wrongly

## gringo/clingo 4.5.0

  * fixed grounding of recursive aggregates
  * fixed usage of lua\_next
  * fixed bug when applying constant definitions
  * updated underlying clasp to version 3.1.1
  * added support for negation in front of relation literals
  * added option --update-domains to cleanup gringo's domains
    using clasp's top-level assignment when solving incrementally
  * added domain inspection to scripting interface
  * added term parser to scripting interface
  * added support for python 3 (experimental)
  * added support for one elementary tuples
  * added support for unary - operator in front of functions and symbols
  * added support for recursive nonmonotone aggregate via translation
  * added program reify to reify logic programs
  * added option to rewrite minimize constaints for use with reify
  * changed inbuilt iclingo mode
    (breaks backwards compatibility)
  * changed handling of pools, intervals, and undefined operations according to AG
    (breaks backwards compatibility)
  * changed handling of ==, it is treated like = now
  * changed SolveFuture.interrupt to SolveFuture.cancel
    (breaks backwards compatibility)

## gringo/clingo 4.4.0

  * updated underlying clasp to version 3.1.0
    * this version brings numerous fixes regarding incremental solving
  * scripting API changes
    * ground takes a list of programs to ground now and immediately starts
      grounding (breaks backwards compatibility)
    * asolve has been renamed to solveAsync
      (breaks backwards compatibility)
    * the solver configuration is better integrated now
      (breaks backwards compatibility)
    * solver statistics are a property now
      (breaks backwards compatibility)
    * added a method to add clauses during solving
    * added load method to load files
    * added solveIter method to iterate over methods without using a callback
    * added optional assumptions to solve/solveAsync/solveIter method
    * enableEnumAssumption became a property
  * added library that can be imported in python
  * rules with fact heads where not simplified in all cases
  * fixed grounding of recursive aggregates
  * fixed translation of aggregates with multiple guards

## gringo/clingo 4.3.0

  * fixed bug with incremental parameters in minimize constraints
  * fixed handling of empty tuples
  * fixed translation of conditional literals
  * fixed translation of factual body aggregates
  * fixed bug not properly recognizing aggregates as non-monotone
  * fixed bug not properly grounding recursive head aggregates
  * fixed bug with recursive negated aggregates
  * fixed bug with head aggregates with multiple elements
  * improved handling of conditional literals
  * added method to get optimization values of model in scripting language
  * clingo uses clasp 3.0 now

## gringo/clingo 4.2.1

  * fixed bug in simplification of aggregates
  * fixed bug with raw strings in macros
  * fixed compilation issues with older glibc versions
  * fixed output for enumeration of cautious consequences
  * fixed bugs in clasp library
    * fixed race in parallel model enumeration
    * fixed incremental optimization
    * fixed cleanup up of learnt constraints during incremental solving
  * workaround for libstdc++'s bad choice for hash<uint64_t> on 32bit arches

## gringo/clingo 4.2

  * added clingo 
    * supports very flexible scripting support
    * can cover iclingo and oclingo functionality now
  * added stack traces to lua error messages
  * added support for incremental optimization
  * improved python error messages
  * renamed gringo.Function to gringo.Fun
  * removed luabind dependency
  * removed boost-python dependency
  * consistently use not instead of #not as keyword for negation
  * fixed translation of conditions in head aggregates
  * fixed replacement of constants
  * fixed grounding of recursive head aggregates
  * fixed translation of head aggregates
  * fixed show statements for CSP variables (condition was ignored)
  * fixed plain text output of body aggregates
  * added a ton of new bugs

## gringo 4.1

  * added scripting languages python and lua
  * added -c option to define constants
  * added constraints over integer variables
    * linear constraints
    * disjoint constraints
    * show statements for constraint variables
    * (experimental and subject to change)
  * improved translation of disjunctions
  * fixed include directives
  * fixed preprocessing of definitions
  * fixed lparse translation of optimization constructs
