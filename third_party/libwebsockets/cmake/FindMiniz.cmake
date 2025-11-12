# This module tries to find miniz library and include files
#
# MINIZ_INCLUDE_DIR, path where to find miniz.h
# MINIZ_LIBRARY_DIR, path where to find libminiz.so
# MINIZ_LIBRARIES, the library to link against
# MINIZ_FOUND, If false, do not try to use miniz
#
# This currently works probably only for Linux

FIND_PATH ( MINIZ_INCLUDE_DIR miniz.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY ( MINIZ_LIBRARIES libminiz.so libminiz.a libminiz.so.2 libminiz.so.0.1
    /usr/local/lib
    /usr/local/lib64
    /usr/lib
    /usr/lib64
)

GET_FILENAME_COMPONENT( MINIZ_LIBRARY_DIR ${MINIZ_LIBRARIES} PATH )

SET ( MINIZ_FOUND "NO" )
IF ( MINIZ_INCLUDE_DIR )
    IF ( MINIZ_LIBRARIES )
        SET ( MINIZ_FOUND "YES" )
    ENDIF ( MINIZ_LIBRARIES )
ENDIF ( MINIZ_INCLUDE_DIR )

MARK_AS_ADVANCED(
    MINIZ_LIBRARY_DIR
    MINIZ_INCLUDE_DIR
    MINIZ_LIBRARIES
)
