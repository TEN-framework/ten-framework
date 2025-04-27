#include "msgpack.h"

const char* msgpack_version(void)
{
    return MSGPACK_VERSION;
}

int msgpack_version_major(void)
{
    return MSGPACK_VERSION_MAJOR;
}

int msgpack_version_minor(void)
{
    return MSGPACK_VERSION_MINOR;
}

int msgpack_version_revision(void)
{
    return MSGPACK_VERSION_REVISION;
}

