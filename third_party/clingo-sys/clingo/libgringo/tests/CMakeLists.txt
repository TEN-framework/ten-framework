# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/graph.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/intervals.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/safetycheck.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/symbol.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/term.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/term_helper.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/utility.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source-group-ground
    "${CMAKE_CURRENT_SOURCE_DIR}/ground/dependency.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/ground/grounder_helper.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/ground/instantiation.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/ground/literal.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/ground/program.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/ground/theory.cc")
source_group("${ide_source_group}\\ground" FILES ${source-group-ground})
set(source-group-input
    "${CMAKE_CURRENT_SOURCE_DIR}/input/aggregate.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/aggregate_helper.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/iesolver.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/lit_helper.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/literal.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/nongroundgrammar.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/nongroundlexer.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/parse_term.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/program.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/programbuilder.cc")
source_group("${ide_source_group}\\input" FILES ${source-group-input})
set(source-group-output
    "${CMAKE_CURRENT_SOURCE_DIR}/output/aspcomp13.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/output/incremental.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/output/lparse.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/output/solver_helper.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/output/theory.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/output/warnings.cc")
source_group("${ide_source_group}\\output" FILES ${source-group-output})
set(source
    ${source-group}
    ${source-group-ground}
    ${source-group-input}
    ${source-group-output})
# ]]]

add_executable(test_gringo ${source})
target_link_libraries(test_gringo PRIVATE libgringo libclasp Catch2::Catch2WithMain)
target_include_directories(test_gringo PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
if(MSVC)
    target_compile_definitions(test_gringo PRIVATE _SCL_SECURE_NO_WARNINGS)
endif()
set_target_properties(test_gringo PROPERTIES FOLDER test)

add_test(NAME test_gringo COMMAND test_gringo)
