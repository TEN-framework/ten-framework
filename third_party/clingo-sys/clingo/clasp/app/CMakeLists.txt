set(files
	main.cpp)
add_executable(clasp ${files})
set_target_properties(clasp PROPERTIES FOLDER exe)
if (NOT CMAKE_INSTALL_BINDIR)
	message(STATUS "BINDIR not set - using bin")
	set(CMAKE_INSTALL_BINDIR "bin")
endif()
if (CLASP_BUILD_STATIC AND UNIX AND NOT APPLE)
	if (CLASP_BUILD_WITH_THREADS)
		string(CONCAT refs "-Wl,-u,pthread_cancel,-u,pthread_cond_broadcast,"
		                   "-u,pthread_cond_destroy,-u,pthread_cond_signal,"
		                   "-u,pthread_cond_timedwait,-u,pthread_cond_wait,"
		                   "-u,pthread_create,-u,pthread_detach,-u,pthread_join,"
		                   "-u,pthread_equal")
		target_link_libraries(clasp ${refs})
	endif()
	target_link_libraries(clasp "-static")
endif()
target_link_libraries(clasp libclasp)

install(TARGETS clasp EXPORT clasp DESTINATION ${CMAKE_INSTALL_BINDIR})
