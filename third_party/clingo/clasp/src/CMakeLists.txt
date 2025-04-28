set(header_path ${CLASP_SOURCE_DIR}/clasp)

# generate config.h
set(CLASP_HAS_THREADS ${CLASP_BUILD_WITH_THREADS})
if (CLASP_BUILD_WITH_THREADS)
set(CLASP_DEFINE_ATOMIC "namespace mt { using std::atomic; }")
set(CLASP_ATOMIC_TYPE "std::atomic")
else()
set(CLASP_DEFINE_ATOMIC "
template <class T>
struct Plain {
	T operator=(T nv)   { return (val_ = nv); }
	operator T () const { return val_; }
	operator T&()       { return val_; }
	T exchange(T nVal)  { T oVal = val_; val_ = nVal; return oVal; }
	T fetch_or(T xVal)  { return exchange(val_ | xVal); }
	T fetch_and(T xVal) { return exchange(val_ & xVal); }
	bool compare_exchange_strong(T& oVal, T nVal) {
		if (val_ == oVal) { val_ = nVal;  return true; }
		else              { oVal = val_;  return false; }
	}
	T val_;
};")
set(CLASP_ATOMIC_TYPE "Clasp::Plain")
endif()
if (NOT DEFINED CLASP_USE_STD_VECTOR)
	if (MSVC)
		set(CLASP_USE_STD_VECTOR "_DEBUG")
	else()
		set(CLASP_USE_STD_VECTOR 0)
	endif()
endif()
configure_file(${header_path}/config.h.in ${CLASP_BINARY_DIR}/clasp/config.h @ONLY)

set(header
	${CLASP_BINARY_DIR}/clasp/config.h
	${header_path}/heuristics.h
	${header_path}/statistics.h
	${header_path}/claspfwd.h
	${header_path}/logic_program.h
	${header_path}/pod_vector.h
	${header_path}/weight_constraint.h
	${header_path}/asp_preprocessor.h
	${header_path}/enumerator.h
	${header_path}/clingo.h
	${header_path}/lookahead.h
	${header_path}/shared_context.h
	${header_path}/solver_strategies.h
	${header_path}/solve_algorithms.h
	${header_path}/literal.h
	${header_path}/satelite.h
	${header_path}/program_builder.h
	${header_path}/dependency_graph.h
	${header_path}/clause.h
	${header_path}/cb_enumerator.h
	${header_path}/minimize_constraint.h
	${header_path}/unfounded_check.h
	${header_path}/solver_types.h
	${header_path}/solver.h
	${header_path}/parser.h
	${header_path}/clasp_facade.h
	${header_path}/model_enumerators.h
	${header_path}/logic_program_types.h
	${header_path}/constraint.h)
set(ide_header "Header Files")
source_group("${ide_header}" FILES ${header})
set(header_util
	${header_path}/util/misc_types.h
	${header_path}/util/multi_queue.h
	${header_path}/util/left_right_sequence.h
	${header_path}/util/pod_vector.h
	${header_path}/util/hash.h
	${header_path}/util/indexed_priority_queue.h
	${header_path}/util/type_manip.h
	${header_path}/util/timer.h)
source_group("${ide_header}\\util" FILES ${header_util})
set(header_cli
	${header_path}/cli/clasp_app.h
	${header_path}/cli/clasp_cli_configs.inl
	${header_path}/cli/clasp_cli_options.inl
	${header_path}/cli/clasp_output.h
	${header_path}/cli/clasp_options.h)
source_group("${ide_header}\\cli" FILES ${header_cli})

set(src
	asp_preprocessor.cpp
	cb_enumerator.cpp
	clasp_app.cpp
	clasp_facade.cpp
	clasp_options.cpp
	clasp_output.cpp
	clause.cpp
	clingo.cpp
	constraint.cpp
	dependency_graph.cpp
	enumerator.cpp
	heuristics.cpp
	logic_program.cpp
	logic_program_types.cpp
	lookahead.cpp
	minimize_constraint.cpp
	model_enumerators.cpp
	parser.cpp
	program_builder.cpp
	satelite.cpp
	shared_context.cpp
	solve_algorithms.cpp
	solver.cpp
	solver_strategies.cpp
	solver_types.cpp
	statistics.cpp
	timer.cpp
	unfounded_check.cpp
	weight_constraint.cpp)
if (CLASP_BUILD_WITH_THREADS)
LIST(APPEND src
	parallel_solve.cpp)
set(header_mt
	${header_path}/mt/thread.h
	${header_path}/mt/mutex.h
	${header_path}/mt/parallel_solve.h)
source_group("${ide_header}\\mt" FILES ${header_mt})
endif()

add_library(libclasp ${header} ${header_util} ${header_cli} ${header_mt} ${src})
if (CLASP_BUILD_WITH_THREADS)
	target_link_libraries(libclasp PUBLIC Threads::Threads)
	target_compile_options(libclasp PRIVATE
		$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
		-Wno-deprecated-declarations>)
endif()
if(MSVC)
	target_compile_definitions(libclasp PRIVATE _SCL_SECURE_NO_WARNINGS)
	target_compile_options(libclasp PRIVATE "$<$<CONFIG:DEBUG>:/W4>")
	set(VC_RELEASE_OPTIONS /Oi /Oy /GL /Gy /GS-)
	target_compile_definitions(libclasp PUBLIC "$<$<CONFIG:RELEASE>:_SECURE_SCL=0>")
	target_compile_options(libclasp PUBLIC "$<$<CONFIG:RELEASE>:${VC_RELEASE_OPTIONS}>")
endif()
target_include_directories(libclasp PUBLIC
	$<BUILD_INTERFACE:${CLASP_SOURCE_DIR}>
	$<BUILD_INTERFACE:${CLASP_BINARY_DIR}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${clasp_include_dest}>)
target_link_libraries(libclasp PUBLIC libpotassco)
set_target_properties(libclasp PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(libclasp PROPERTIES
	OUTPUT_NAME clasp
	FOLDER lib)

# installation
if (CLASP_INSTALL_LIB)
	install(TARGETS libclasp EXPORT ClaspTargets DESTINATION "${CMAKE_INSTALL_LIBDIR}/${clasp_library_dest}")
	install(FILES ${header}                      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${clasp_include_dest}/clasp")
	install(FILES ${header_util}                 DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${clasp_include_dest}/clasp/util")
	install(FILES ${header_cli}                  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${clasp_include_dest}/clasp/cli")
	if (header_mt)
		install(FILES ${header_mt}               DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${clasp_include_dest}/clasp/mt")
	endif()
endif()

