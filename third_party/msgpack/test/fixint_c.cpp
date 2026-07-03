#include <msgpack.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif //defined(__GNUC__)

#include <gtest/gtest.h>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif //defined(__GNUC__)

TEST(fixint, size)
{
    msgpack_sbuffer* sbuf = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new(sbuf, msgpack_sbuffer_write);

    size_t sum = 0;

    EXPECT_EQ(0, msgpack_pack_fix_int8(pk, 0));
    EXPECT_EQ(sum+=2, sbuf->size);
    EXPECT_EQ(0, msgpack_pack_fix_int16(pk, 0));
    EXPECT_EQ(sum+=3, sbuf->size);
    EXPECT_EQ(0, msgpack_pack_fix_int32(pk, 0));
    EXPECT_EQ(sum+=5, sbuf->size);
    EXPECT_EQ(0, msgpack_pack_fix_int64(pk, 0));
    EXPECT_EQ(sum+=9, sbuf->size);

    EXPECT_EQ(0, msgpack_pack_fix_uint8(pk, 0));
    EXPECT_EQ(sum+=2, sbuf->size);
    EXPECT_EQ(0, msgpack_pack_fix_uint16(pk, 0));
    EXPECT_EQ(sum+=3, sbuf->size);
    EXPECT_EQ(0, msgpack_pack_fix_uint32(pk, 0));
    EXPECT_EQ(sum+=5, sbuf->size);
    EXPECT_EQ(0, msgpack_pack_fix_uint64(pk, 0));
    EXPECT_EQ(sum+=9, sbuf->size);

    msgpack_sbuffer_free(sbuf);
    msgpack_packer_free(pk);
}
