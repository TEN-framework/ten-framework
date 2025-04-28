# [[[header: .
set(ide_header_group "Header Files")
set(header-group-gringo
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/backend.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/backtrace.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/base.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/bug.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/clonable.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/comparable.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/domain.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/graph.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/hash_set.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/hashable.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/indexed.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/intervals.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/lexerstate.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/locatable.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/logger.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/printable.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/safetycheck.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/symbol.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/term.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/terms.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/types.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/utility.hh")
source_group("${ide_header_group}\\gringo" FILES ${header-group-gringo})
set(header-group-gringo-ground
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/binders.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/dependency.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/instantiation.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/literal.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/literals.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/program.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/statement.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/statements.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/ground/types.hh")
source_group("${ide_header_group}\\gringo\\ground" FILES ${header-group-gringo-ground})
set(header-group-gringo-input
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/aggregate.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/aggregates.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/groundtermparser.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/literal.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/literals.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/nongroundparser.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/program.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/programbuilder.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/statement.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/theory.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/input/types.hh")
source_group("${ide_header_group}\\gringo\\input" FILES ${header-group-gringo-input})
set(header-group-gringo-output
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/aggregates.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/backends.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/literal.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/literals.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/output.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/statement.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/statements.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/theory.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/gringo/output/types.hh")
source_group("${ide_header_group}\\gringo\\output" FILES ${header-group-gringo-output})
set(header
    ${header-group-gringo}
    ${header-group-gringo-ground}
    ${header-group-gringo-input}
    ${header-group-gringo-output})
# ]]]
# [[[source: ./src
bison_target_or_gen("src/input/groundtermgrammar.yy")
re2c_target_or_gen("/src/input/groundtermlexer.xh")
bison_target_or_gen("src/input/nongroundgrammar.yy")
re2c_target_or_gen("/src/input/nongroundlexer.xch")
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/src/symbol.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/term.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terms.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source-group-ground
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ground/instantiation.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ground/literal.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ground/literals.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ground/program.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ground/statements.cc")
source_group("${ide_source_group}\\ground" FILES ${source-group-ground})
set(source-group-input
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/aggregate.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/aggregates.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/groundtermlexer.xh"
    ${RE2C_groundtermlexer_OUTPUT}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/groundtermparser.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/literal.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/literals.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/nongroundlexer.xch"
    ${RE2C_nongroundlexer_OUTPUT}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/nongroundparser.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/program.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/programbuilder.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/statement.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/theory.cc")
source_group("${ide_source_group}\\input" FILES ${source-group-input})
set(source-group-input-groundtermgrammar
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/groundtermgrammar.yy"
    ${BISON_groundtermgrammar_OUTPUTS})
source_group("${ide_source_group}\\input\\groundtermgrammar" FILES ${source-group-input-groundtermgrammar})
set(source-group-input-nongroundgrammar
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/nongroundgrammar.yy"
    ${BISON_nongroundgrammar_OUTPUTS})
source_group("${ide_source_group}\\input\\nongroundgrammar" FILES ${source-group-input-nongroundgrammar})
set(source-group-output
    "${CMAKE_CURRENT_SOURCE_DIR}/src/output/aggregates.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/output/literal.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/output/literals.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/output/output.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/output/statement.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/output/statements.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/output/theory.cc")
source_group("${ide_source_group}\\output" FILES ${source-group-output})
set(source
    ${source-group}
    ${source-group-ground}
    ${source-group-input}
    ${source-group-input-groundtermgrammar}
    ${source-group-input-nongroundgrammar}
    ${source-group-output})
# ]]]

add_library(libgringo STATIC ${header} ${source})
target_link_libraries(libgringo PUBLIC libpotassco libreify tsl::ordered_map tsl::hopscotch_map tsl::sparse_map tl::optional mpark::variant math::wide_integer)
target_include_directories(libgringo
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/input/clingopath.hh.in" "${CMAKE_CURRENT_BINARY_DIR}/src/input/clingopath.hh")
if (CLINGO_MAP_TYPE STREQUAL "hopscotch")
    target_compile_definitions(libgringo PUBLIC CLINGO_MAP_TYPE=0)
elseif (CLINGO_MAP_TYPE STREQUAL "sparse")
    target_compile_definitions(libgringo PUBLIC CLINGO_MAP_TYPE=1)
endif()
if (NOT CLINGO_BUILD_SHARED)
    target_compile_definitions(libgringo PUBLIC CLINGO_NO_VISIBILITY)
endif()
if(MSVC)
    target_compile_definitions(libgringo PRIVATE _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/ground/statements.cc"
        PROPERTIES COMPILE_FLAGS "/bigobj")
elseif(MINGW OR CYGWIN OR MSYS)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/ground/statements.cc"
        PROPERTIES COMPILE_FLAGS "-Wa,-mbig-obj")
endif()
set_target_properties(libgringo PROPERTIES
    OUTPUT_NAME gringo
    FOLDER lib)

if (CLINGO_BUILD_TESTS)
    add_subdirectory(tests)
    # NOTE: the tests must have access to the privately generated parsers
    target_include_directories(test_gringo PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>")
endif()

