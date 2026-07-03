# The strategy is to only export to PARENT_SCOPE
#
#  - changes to LIB_LIST
#  - includes via include_directories
#
# and keep everything else private

include_directories(.)

# configure or find systemd library
set(LIB_SYSTEMD_LIBRARIES CACHE PATH "Path to the libsystemd library")
if ("${LWS_SYSTEMD_LIBRARIES}" STREQUAL "")
    if (NOT LIB_SYSTEMD_FOUND)
        find_path(LIBSYSTEMD_INCLUDE_DIRS NAMES systemd/sd-event.h)
        find_library(LIBSYSTEMD_LIBRARIES NAMES systemd)
    endif()
else()
    set(LIBSYSTEMD_LIBRARIES ${LWS_SYSTEMD_LIBRARIES})
    set(LIBSYSTEMD_INCLUDE_DIRS ${LWS_LIBSYSTEMD_INCLUDE_DIRS})
endif()
message("libsystemd include dir: ${LIBSYSTEMD_INCLUDE_DIRS}")
message("libsystemd libraries: ${LIBSYSTEMD_LIBRARIES}")

if (LWS_WITH_EVLIB_PLUGINS)

    create_evlib_plugin(
            evlib_sd
            sdevent.c
            private-lib-event-libs-sdevent.h
            ${LIBSYSTEMD_LIBRARIES}
    )

else()

    list(APPEND LIB_LIST ${LIBSYSTEMD_LIBRARIES})
    list(APPEND SOURCES event-libs/sdevent/sdevent.c)

endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
