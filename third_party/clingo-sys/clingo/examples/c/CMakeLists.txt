set(CMAKE_C_STANDARD 11)

add_executable(model "${CMAKE_CURRENT_SOURCE_DIR}/model.c")
target_link_libraries(model PRIVATE libclingo)
set_target_properties(model PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(version "${CMAKE_CURRENT_SOURCE_DIR}/version.c")
target_link_libraries(version PRIVATE libclingo)
set_target_properties(version PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(backend "${CMAKE_CURRENT_SOURCE_DIR}/backend.c")
target_link_libraries(backend PRIVATE libclingo)
set_target_properties(backend PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(control "${CMAKE_CURRENT_SOURCE_DIR}/control.c")
target_link_libraries(control PRIVATE libclingo)
set_target_properties(control PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(propagator "${CMAKE_CURRENT_SOURCE_DIR}/propagator.c")
target_link_libraries(propagator PRIVATE libclingo)
set_target_properties(propagator PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(symbolic-atoms "${CMAKE_CURRENT_SOURCE_DIR}/symbolic-atoms.c")
target_link_libraries(symbolic-atoms PRIVATE libclingo)
set_target_properties(symbolic-atoms PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(configuration "${CMAKE_CURRENT_SOURCE_DIR}/configuration.c")
target_link_libraries(configuration PRIVATE libclingo)
set_target_properties(configuration PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(statistics "${CMAKE_CURRENT_SOURCE_DIR}/statistics.c")
target_link_libraries(statistics PRIVATE libclingo)
set_target_properties(statistics PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(symbol "${CMAKE_CURRENT_SOURCE_DIR}/symbol.c")
target_link_libraries(symbol PRIVATE libclingo)
set_target_properties(symbol PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(ast "${CMAKE_CURRENT_SOURCE_DIR}/ast.c")
target_link_libraries(ast PRIVATE libclingo)
set_target_properties(ast PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(solve-async "${CMAKE_CURRENT_SOURCE_DIR}/solve-async.c")
target_link_libraries(solve-async PRIVATE libclingo)
set_target_properties(solve-async PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(theory-atoms "${CMAKE_CURRENT_SOURCE_DIR}/theory-atoms.c")
target_link_libraries(theory-atoms PRIVATE libclingo)
set_target_properties(theory-atoms PROPERTIES FOLDER "exe/clingo-c-examples")

add_executable(application "${CMAKE_CURRENT_SOURCE_DIR}/application.c")
target_link_libraries(application PRIVATE libclingo)
set_target_properties(application PROPERTIES FOLDER "exe/application")
