include_directories(.)

if (LWS_WITH_SYS_METRICS)
	list(APPEND SOURCES
		system/metrics/metrics.c
	)
endif()

exports_to_parent_scope()

