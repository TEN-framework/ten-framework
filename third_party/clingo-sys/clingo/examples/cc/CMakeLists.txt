add_executable(cc-solve-iteratively "solve-iteratively.cc")
target_link_libraries(cc-solve-iteratively PRIVATE libclingo)
set_target_properties(cc-solve-iteratively PROPERTIES FOLDER "exe/clingo-cc-examples")

add_executable(cc-inject-terms "inject-terms.cc")
target_link_libraries(cc-inject-terms PRIVATE libclingo)
set_target_properties(cc-inject-terms PROPERTIES FOLDER "exe/clingo-cc-examples")

add_executable(cc-heuristic "heuristic.cc")
target_link_libraries(cc-heuristic PRIVATE libclingo)
set_target_properties(cc-heuristic PROPERTIES FOLDER "exe/clingo-cc-examples")
