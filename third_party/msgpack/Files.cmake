# Source files
SET (msgpack-c_SOURCES
    src/objectc.c
    src/unpack.c
    src/version.c
    src/vrefbuffer.c
    src/zone.c
)

# Header files
SET (msgpack-c_common_HEADERS
    include/msgpack.h
    include/msgpack/fbuffer.h
    include/msgpack/gcc_atomic.h
    include/msgpack/object.h
    include/msgpack/pack.h
    include/msgpack/pack_define.h
    include/msgpack/sbuffer.h
    include/msgpack/timestamp.h
    include/msgpack/unpack.h
    include/msgpack/unpack_define.h
    include/msgpack/unpack_template.h
    include/msgpack/util.h
    include/msgpack/version.h
    include/msgpack/version_master.h
    include/msgpack/vrefbuffer.h
    include/msgpack/zbuffer.h
    include/msgpack/zone.h
)

# Header files will configured
SET (msgpack-c_configured_HEADERS
    include/msgpack/pack_template.h
    include/msgpack/sysdep.h
)

# All header files
LIST (APPEND msgpack-c_HEADERS
    ${msgpack-c_common_HEADERS}
    ${msgpack-c_configured_HEADERS}
)
