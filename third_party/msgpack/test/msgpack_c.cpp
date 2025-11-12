#include "msgpack.h"

#include <math.h>
#include <vector>
#include <limits>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif //defined(__GNUC__)

#include <gtest/gtest.h>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif //defined(__GNUC__)

#if defined(_MSC_VER) || defined(__MINGW32__)
#define msgpack_rand() ((double)rand() / RAND_MAX)
#else  // _MSC_VER || __MINGW32__
#define msgpack_rand() drand48()
#endif // _MSC_VER || __MINGW32__

#if defined(_MSC_VER)
#define msgpack_snprintf sprintf_s
#else  // _MSC_VER
#define msgpack_snprintf snprintf
#endif // _MSC_VER

using namespace std;

const unsigned int kLoop = 10000;
const double kEPS = 1e-10;

#define GEN_TEST_SIGNED(test_type, func_type)                           \
    do {                                                                \
        vector<test_type> v;                                            \
        v.push_back(0);                                                 \
        v.push_back(1);                                                 \
        v.push_back(-1);                                                \
        v.push_back(numeric_limits<test_type>::min());                  \
        v.push_back(numeric_limits<test_type>::max());                  \
        for (unsigned int i = 0; i < kLoop; i++)                        \
            v.push_back(static_cast<test_type>(rand()));                \
        for (unsigned int i = 0; i < v.size() ; i++) {                  \
            test_type val = v[i];                                       \
            msgpack_sbuffer sbuf;                                       \
            msgpack_sbuffer_init(&sbuf);                                \
            msgpack_packer pk;                                          \
            msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);     \
            msgpack_pack_##func_type(&pk, val);                         \
            msgpack_zone z;                                             \
            msgpack_zone_init(&z, 2048);                                \
            msgpack_object obj;                                         \
            msgpack_unpack_return ret =                                 \
                msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);   \
            EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);                     \
            if (val < 0) {                                              \
                EXPECT_EQ(MSGPACK_OBJECT_NEGATIVE_INTEGER, obj.type);   \
                EXPECT_EQ(val, obj.via.i64);                            \
            } else {                                                    \
                EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.type);   \
                EXPECT_EQ(static_cast<uint64_t>(val), obj.via.u64);     \
            }                                                           \
            msgpack_zone_destroy(&z);                                   \
            msgpack_sbuffer_destroy(&sbuf);                             \
        }                                                               \
    } while(0)

#define GEN_TEST_UNSIGNED(test_type, func_type)                         \
    do {                                                                \
        vector<test_type> v;                                            \
        v.push_back(0);                                                 \
        v.push_back(1);                                                 \
        v.push_back(2);                                                 \
        v.push_back(numeric_limits<test_type>::min());                  \
        v.push_back(numeric_limits<test_type>::max());                  \
        for (unsigned int i = 0; i < kLoop; i++)                        \
            v.push_back(static_cast<test_type>(rand()));                \
        for (unsigned int i = 0; i < v.size() ; i++) {                  \
            test_type val = v[i];                                       \
            msgpack_sbuffer sbuf;                                       \
            msgpack_sbuffer_init(&sbuf);                                \
            msgpack_packer pk;                                          \
            msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);     \
            msgpack_pack_##func_type(&pk, val);                         \
            msgpack_zone z;                                             \
            msgpack_zone_init(&z, 2048);                                \
            msgpack_object obj;                                         \
            msgpack_unpack_return ret =                                 \
                msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);   \
            EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);                     \
            EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.type);       \
            EXPECT_EQ(val, obj.via.u64);                                \
            msgpack_zone_destroy(&z);                                   \
            msgpack_sbuffer_destroy(&sbuf);                             \
        }                                                               \
    } while(0)

TEST(MSGPACKC, simple_buffer_char)
{
#if defined(CHAR_MIN)
#if CHAR_MIN < 0
    GEN_TEST_SIGNED(char, char);
#else
    GEN_TEST_UNSIGNED(char, char);
#endif
#else
#error CHAR_MIN is not defined
#endif
}

TEST(MSGPACKC, simple_buffer_singed_char)
{
    GEN_TEST_SIGNED(signed char, signed_char);
}

TEST(MSGPACKC, simple_buffer_short)
{
    GEN_TEST_SIGNED(short, short);
}

TEST(MSGPACKC, simple_buffer_int)
{
    GEN_TEST_SIGNED(int, int);
}

TEST(MSGPACKC, simple_buffer_long)
{
    GEN_TEST_SIGNED(long, long);
}

TEST(MSGPACKC, simple_buffer_long_long)
{
    GEN_TEST_SIGNED(long long, long_long);
}

TEST(MSGPACKC, simple_buffer_unsigned_char)
{
    GEN_TEST_UNSIGNED(unsigned char, unsigned_char);
}

TEST(MSGPACKC, simple_buffer_unsigned_short)
{
    GEN_TEST_UNSIGNED(unsigned short, unsigned_short);
}

TEST(MSGPACKC, simple_buffer_unsigned_int)
{
    GEN_TEST_UNSIGNED(unsigned int, unsigned_int);
}

TEST(MSGPACKC, simple_buffer_unsigned_long)
{
    GEN_TEST_UNSIGNED(unsigned long, unsigned_long);
}

TEST(MSGPACKC, simple_buffer_unsigned_long_long)
{
    GEN_TEST_UNSIGNED(unsigned long long, unsigned_long_long);
}

TEST(MSGPACKC, simple_buffer_uint8)
{
    GEN_TEST_UNSIGNED(uint8_t, uint8);
}

TEST(MSGPACKC, simple_buffer_uint16)
{
    GEN_TEST_UNSIGNED(uint16_t, uint16);
}

TEST(MSGPACKC, simple_buffer_uint32)
{
    GEN_TEST_UNSIGNED(uint32_t, uint32);
}

TEST(MSGPACKC, simple_buffer_uint64)
{
    GEN_TEST_UNSIGNED(uint64_t, uint64);
}

TEST(MSGPACKC, simple_buffer_int8)
{
    GEN_TEST_SIGNED(int8_t, int8);
}

TEST(MSGPACKC, simple_buffer_int16)
{
    GEN_TEST_SIGNED(int16_t, int16);
}

TEST(MSGPACKC, simple_buffer_int32)
{
    GEN_TEST_SIGNED(int32_t, int32);
}

TEST(MSGPACKC, simple_buffer_int64)
{
    GEN_TEST_SIGNED(int64_t, int64);
}

#if !defined(_MSC_VER) || _MSC_VER >=1800

TEST(MSGPACKC, simple_buffer_float)
{
    vector<float> v;
    v.push_back(0.0);
    v.push_back(1.0);
    v.push_back(-1.0);
    v.push_back(numeric_limits<float>::min());
    v.push_back(numeric_limits<float>::max());
    v.push_back(nanf("tag"));
    if (numeric_limits<float>::has_infinity) {
        v.push_back(numeric_limits<float>::infinity());
        v.push_back(-numeric_limits<float>::infinity());
    }
    if (numeric_limits<float>::has_quiet_NaN) {
        v.push_back(numeric_limits<float>::quiet_NaN());
    }
    if (numeric_limits<float>::has_signaling_NaN) {
        v.push_back(numeric_limits<float>::signaling_NaN());
    }

    for (unsigned int i = 0; i < kLoop; i++) {
        v.push_back(static_cast<float>(msgpack_rand()));
        v.push_back(static_cast<float>(-msgpack_rand()));
    }

    for (unsigned int i = 0; i < v.size() ; i++) {
        float val = v[i];
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
        msgpack_pack_float(&pk, val);
        msgpack_zone z;
        msgpack_zone_init(&z, 2048);
        msgpack_object obj;
        msgpack_unpack_return ret =
            msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
        EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
        EXPECT_EQ(MSGPACK_OBJECT_FLOAT32, obj.type);
        if (isnan(val)) {
            EXPECT_TRUE(isnan(obj.via.f64));
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
            EXPECT_TRUE(isnan(obj.via.dec));
#endif // MSGPACK_USE_LEGACY_NAME_AS_FLOAT
        }
        else if (isinf(val)) {
            EXPECT_TRUE(isinf(obj.via.f64));
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
            EXPECT_TRUE(isinf(obj.via.dec));
#endif // MSGPACK_USE_LEGACY_NAME_AS_FLOAT
        }
        else {
            EXPECT_TRUE(fabs(obj.via.f64 - val) <= kEPS);
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
            EXPECT_TRUE(fabs(obj.via.dec - val) <= kEPS);
#endif // MSGPACK_USE_LEGACY_NAME_AS_FLOAT
        }
        msgpack_zone_destroy(&z);
        msgpack_sbuffer_destroy(&sbuf);
    }
}

TEST(MSGPACKC, simple_buffer_double)
{
    vector<double> v;
    v.push_back(0.0);
    v.push_back(-0.0);
    v.push_back(1.0);
    v.push_back(-1.0);
    v.push_back(numeric_limits<double>::min());
    v.push_back(numeric_limits<double>::max());
    v.push_back(nan("tag"));
    if (numeric_limits<double>::has_infinity) {
        v.push_back(numeric_limits<double>::infinity());
        v.push_back(-numeric_limits<double>::infinity());
    }
    if (numeric_limits<double>::has_quiet_NaN) {
        v.push_back(numeric_limits<double>::quiet_NaN());
    }
    if (numeric_limits<double>::has_signaling_NaN) {
        v.push_back(numeric_limits<double>::signaling_NaN());
    }
    for (unsigned int i = 0; i < kLoop; i++) {
        v.push_back(msgpack_rand());
        v.push_back(-msgpack_rand());
    }

    for (unsigned int i = 0; i < v.size() ; i++) {
        double val = v[i];
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
        msgpack_pack_double(&pk, val);
        msgpack_zone z;
        msgpack_zone_init(&z, 2048);
        msgpack_object obj;
        msgpack_unpack_return ret =
            msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
        EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
        EXPECT_EQ(MSGPACK_OBJECT_FLOAT64, obj.type);
        EXPECT_EQ(MSGPACK_OBJECT_FLOAT, obj.type);
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
        EXPECT_EQ(MSGPACK_OBJECT_DOUBLE, obj.type);
#endif // MSGPACK_USE_LEGACY_NAME_AS_FLOAT
        if (isnan(val)) {
            EXPECT_TRUE(isnan(obj.via.f64));
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
            EXPECT_TRUE(isnan(obj.via.dec));
#endif // MSGPACK_USE_LEGACY_NAME_AS_FLOAT
        }
        else if (isinf(val)) {
            EXPECT_TRUE(isinf(obj.via.f64));
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
            EXPECT_TRUE(isinf(obj.via.dec));
#endif // MSGPACK_USE_LEGACY_NAME_AS_FLOAT
        }
        else {
            EXPECT_TRUE(fabs(obj.via.f64 - val) <= kEPS);
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
            EXPECT_TRUE(fabs(obj.via.dec - val) <= kEPS);
#endif // MSGPACK_USE_LEGACY_NAME_AS_FLOAT
        }
        msgpack_zone_destroy(&z);
        msgpack_sbuffer_destroy(&sbuf);
    }
}

#endif // !defined(_MSC_VER) || _MSC_VER >=1800

TEST(MSGPACKC, simple_buffer_nil)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_nil(&pk);
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_NIL, obj.type);
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_true)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_true(&pk);
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, obj.type);
    EXPECT_EQ(true, obj.via.boolean);
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_false)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_false(&pk);
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, obj.type);
    EXPECT_FALSE(obj.via.boolean);
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext1)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    char const buf[] = { 2 };

    msgpack_pack_ext(&pk, sizeof(buf), 1);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(1u, obj.via.ext.size);
    EXPECT_EQ(1, obj.via.ext.type);
    EXPECT_EQ(2, obj.via.ext.ptr[0]);
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext2)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    char const buf[] = { 2, 3 };

    msgpack_pack_ext(&pk, sizeof(buf), 0);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(2u, obj.via.ext.size);
    EXPECT_EQ(0, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext4)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    char const buf[] = { 2, 3, 4, 5 };

    msgpack_pack_ext(&pk, sizeof(buf), 1);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(4u, obj.via.ext.size);
    EXPECT_EQ(1, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext8)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    char const buf [] = { 2, 3, 4, 5, 6, 7, 8, 9 };

    msgpack_pack_ext(&pk, sizeof(buf), 1);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(8u, obj.via.ext.size);
    EXPECT_EQ(1, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext16)
{
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    char const buf [] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };

    msgpack_pack_ext(&pk, sizeof(buf), 1);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(16u, obj.via.ext.size);
    EXPECT_EQ(1, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext_1byte_0)
{
    const size_t size = 0;
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_ext(&pk, size, 77);
    // fprintf(stderr, "size: %u, data: \"", sbuf.size);
    // for (size_t i = 0; i < sbuf.size; i++)
    //   fprintf(stderr, "%02x ", (uint8_t)sbuf.data[i]);
    // fprintf(stderr, "\"\n");
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(size, obj.via.ext.size);
    EXPECT_EQ(77, obj.via.ext.type);
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext_1byte_255)
{
    const size_t size = 255;
    char buf[size];
    for (size_t i = 0; i != size; ++i) buf[i] = (char)i;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_ext(&pk, size, 78);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(size, obj.via.ext.size);
    EXPECT_EQ(78, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext_2byte_256)
{
    const size_t size = 256;
    char buf[size];
    for (size_t i = 0; i != size; ++i) buf[i] = (char)i;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_ext(&pk, size, 79);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(size, obj.via.ext.size);
    EXPECT_EQ(79, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext_2byte_65535)
{
    const size_t size = 65535;
    char buf[size];
    for (size_t i = 0; i != size; ++i) buf[i] = (char)i;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_ext(&pk, size, 80);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(size, obj.via.ext.size);
    EXPECT_EQ(80, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_fixext_4byte_65536)
{
    const size_t size = 65536;
    char buf[size];
    for (size_t i = 0; i != size; ++i) buf[i] = (char)i;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_ext(&pk, size, 81);
    msgpack_pack_ext_body(&pk, buf, sizeof(buf));
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(size, obj.via.ext.size);
    EXPECT_EQ(81, obj.via.ext.type);
    EXPECT_EQ(0, memcmp(buf, obj.via.ext.ptr, sizeof(buf)));
    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_timestamp_32)
{
    msgpack_timestamp ts = {
        0xffffffff,
        0
    };

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_timestamp(&pk, &ts);
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(4u, obj.via.ext.size);
    EXPECT_EQ(-1, obj.via.ext.type);
    msgpack_timestamp ts2;
    bool r = msgpack_object_to_timestamp(&obj, &ts2);

    EXPECT_TRUE(r);
    EXPECT_EQ(ts.tv_sec, ts2.tv_sec);
    EXPECT_EQ(ts.tv_nsec, ts2.tv_nsec);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_timestamp_64)
{
    msgpack_timestamp ts = {
        0x3ffffffffL,
        999999999
    };

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_timestamp(&pk, &ts);
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(8u, obj.via.ext.size);
    EXPECT_EQ(-1, obj.via.ext.type);
    msgpack_timestamp ts2;
    bool r = msgpack_object_to_timestamp(&obj, &ts2);

    EXPECT_TRUE(r);
    EXPECT_EQ(ts.tv_sec, ts2.tv_sec);
    EXPECT_EQ(ts.tv_nsec, ts2.tv_nsec);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_timestamp_96)
{
    msgpack_timestamp ts = {
        0x7fffffffffffffffLL,
        999999999
    };

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_timestamp(&pk, &ts);
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret =
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(12u, obj.via.ext.size);
    EXPECT_EQ(-1, obj.via.ext.type);
    msgpack_timestamp ts2;
    bool r = msgpack_object_to_timestamp(&obj, &ts2);

    EXPECT_TRUE(r);
    EXPECT_EQ(ts.tv_sec, ts2.tv_sec);
    EXPECT_EQ(ts.tv_nsec, ts2.tv_nsec);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_array)
{
    unsigned int array_size = 5;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_array(&pk, array_size);
    msgpack_pack_nil(&pk);
    msgpack_pack_true(&pk);
    msgpack_pack_false(&pk);
    msgpack_pack_int(&pk, 10);
    msgpack_pack_int(&pk, -10);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_ARRAY, obj.type);
    EXPECT_EQ(array_size, obj.via.array.size);

    for (unsigned int i = 0; i < obj.via.array.size; i++) {
        msgpack_object o = obj.via.array.ptr[i];
        switch (i) {
        case 0:
            EXPECT_EQ(MSGPACK_OBJECT_NIL, o.type);
            break;
        case 1:
            EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, o.type);
            EXPECT_EQ(true, o.via.boolean);
            break;
        case 2:
            EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, o.type);
            EXPECT_FALSE(o.via.boolean);
            break;
        case 3:
            EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, o.type);
            EXPECT_EQ(10u, o.via.u64);
            break;
        case 4:
            EXPECT_EQ(MSGPACK_OBJECT_NEGATIVE_INTEGER, o.type);
            EXPECT_EQ(-10, o.via.i64);
            break;
        }
    }

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_map)
{
    unsigned int map_size = 2;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_map(&pk, map_size);
    msgpack_pack_true(&pk);
    msgpack_pack_false(&pk);
    msgpack_pack_int(&pk, 10);
    msgpack_pack_int(&pk, -10);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_MAP, obj.type);
    EXPECT_EQ(map_size, obj.via.map.size);

    for (unsigned int i = 0; i < map_size; i++) {
        msgpack_object key = obj.via.map.ptr[i].key;
        msgpack_object val = obj.via.map.ptr[i].val;
        switch (i) {
        case 0:
            EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, key.type);
            EXPECT_EQ(true, key.via.boolean);
            EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, val.type);
            EXPECT_FALSE(val.via.boolean);
            break;
        case 1:
            EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, key.type);
            EXPECT_EQ(10u, key.via.u64);
            EXPECT_EQ(MSGPACK_OBJECT_NEGATIVE_INTEGER, val.type);
            EXPECT_EQ(-10, val.via.i64);
            break;
        }
    }

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str)
{
    unsigned int str_size = 7;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, "fr", 2);
    msgpack_pack_str_body(&pk, "syuki", 5);
    // invalid data
    msgpack_pack_str_body(&pk, "", 0);
    msgpack_pack_str_body(&pk, "kzk", 0);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str_fix_l)
{
    char const* str = NULL;
    unsigned int str_size = 0;
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0x01u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xa0u));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str_fix_h)
{
    char str[0x1f] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0x1f+1u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xbfu));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str_8_l)
{
    char str[0x1f+1] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0x1f+1+2u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xd9u));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0x20u));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str_8_h)
{
    char str[0xff] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0xff+2u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xd9u));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0xffu));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str_16_l)
{
    char str[0xff+1] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0xff+1+3u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xdau));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0x01u));
    EXPECT_EQ(sbuf.data[2], static_cast<char>(0x00u));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str_16_h)
{
    char str[0xffff] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0xffff+3u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xdau));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0xffu));
    EXPECT_EQ(sbuf.data[2], static_cast<char>(0xffu));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_str_32_l)
{
    char str[0xffff+1] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0xffff+1+5u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xdbu));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0x00u));
    EXPECT_EQ(sbuf.data[2], static_cast<char>(0x01u));
    EXPECT_EQ(sbuf.data[3], static_cast<char>(0x00u));
    EXPECT_EQ(sbuf.data[4], static_cast<char>(0x00u));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_v4raw_fix_l)
{
    char const* str = NULL;
    unsigned int str_size = 0;
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_v4raw(&pk, str_size);
    msgpack_pack_v4raw_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0x01u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xa0u));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_v4raw_fix_h)
{
    char str[0x1f] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_v4raw(&pk, str_size);
    msgpack_pack_v4raw_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0x1f+1u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xbfu));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_v4raw_16_l)
{
    char str[0x1f+1] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_v4raw(&pk, str_size);
    msgpack_pack_v4raw_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0x1f+1+3u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xdau));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0x00u));
    EXPECT_EQ(sbuf.data[2], static_cast<char>(0x20u));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_v4raw_16_h)
{
    char str[0xffff] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_v4raw(&pk, str_size);
    msgpack_pack_v4raw_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0xffff+3u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xdau));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0xffu));
    EXPECT_EQ(sbuf.data[2], static_cast<char>(0xffu));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_buffer_v4raw_32_l)
{
    char str[0xffff+1] = {'0'};
    unsigned int str_size = sizeof(str);
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_v4raw(&pk, str_size);
    msgpack_pack_v4raw_body(&pk, str, str_size);
    EXPECT_EQ(sbuf.size, 0xffff+1+5u);
    EXPECT_EQ(sbuf.data[0], static_cast<char>(0xdbu));
    EXPECT_EQ(sbuf.data[1], static_cast<char>(0x00u));
    EXPECT_EQ(sbuf.data[2], static_cast<char>(0x01u));
    EXPECT_EQ(sbuf.data[3], static_cast<char>(0x00u));
    EXPECT_EQ(sbuf.data[4], static_cast<char>(0x00u));

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp(str, obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_object_print_buffer_str_empty)
{
    unsigned int str_size = 0;
    char buffer[64];

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, "", str_size);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);

    msgpack_object_print_buffer(buffer, sizeof(buffer) - 1, obj);
    EXPECT_STREQ("\"\"", buffer);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_object_print_buffer_array_str)
{
    const char * str = "hello";
    const size_t str_size = strlen(str);
    const unsigned int array_size = 1;
    char expected[64];
    char buffer[64];

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_array(&pk, array_size);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, str, str_size);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_ARRAY, obj.type);
    EXPECT_EQ(array_size, obj.via.array.size);

    msgpack_object o = *obj.via.array.ptr;
    EXPECT_EQ(MSGPACK_OBJECT_STR, o.type);
    EXPECT_EQ(str_size, o.via.str.size);
    EXPECT_EQ(0, memcmp(str, o.via.str.ptr, str_size));

    msgpack_snprintf(expected, sizeof(expected), "[\"%s\"]", str);
    expected[sizeof(expected) - 1] = '\0'; // not needed w/ sprintf_s
    msgpack_object_print_buffer(buffer, sizeof(buffer) - 1, obj);
    EXPECT_STREQ(expected, buffer);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_object_print_buffer_array_str_empty)
{
    const unsigned int array_size = 1;
    const unsigned int str_size = 0;
    char buffer[64];

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_array(&pk, array_size);
    msgpack_pack_str(&pk, str_size);
    msgpack_pack_str_body(&pk, "", 0);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_ARRAY, obj.type);
    EXPECT_EQ(array_size, obj.via.array.size);

    msgpack_object o = *obj.via.array.ptr;
    EXPECT_EQ(MSGPACK_OBJECT_STR, o.type);
    EXPECT_EQ(str_size, o.via.str.size);

    msgpack_object_print_buffer(buffer, sizeof(buffer) - 1, obj);
    EXPECT_STREQ("[\"\"]", buffer);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_object_print_buffer_map_str)
{
    const char * mkey = "key";
    const char * mval = "value";
    char expected[64];
    char buffer[64];
    const size_t mkey_size = strlen(mkey);;
    const size_t mval_size = strlen(mval);
    const unsigned int map_size = 1;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_map(&pk, map_size);
    msgpack_pack_str(&pk, mkey_size);
    msgpack_pack_str_body(&pk, mkey, mkey_size);
    msgpack_pack_str(&pk, mval_size);
    msgpack_pack_str_body(&pk, mval, mval_size);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_MAP, obj.type);
    EXPECT_EQ(map_size, obj.via.map.size);

    msgpack_object key = obj.via.map.ptr->key;
    msgpack_object val = obj.via.map.ptr->val;
    EXPECT_EQ(MSGPACK_OBJECT_STR, key.type);
    EXPECT_EQ(mkey_size, key.via.str.size);
    EXPECT_EQ(0, memcmp(mkey, key.via.str.ptr, mkey_size));
    EXPECT_EQ(MSGPACK_OBJECT_STR, val.type);
    EXPECT_EQ(mval_size, val.via.str.size);
    EXPECT_EQ(0, memcmp(mval, val.via.str.ptr, mval_size));

    msgpack_snprintf(expected, sizeof(expected), "{\"%s\"=>\"%s\"}", mkey, mval);
    expected[sizeof(expected) - 1] = '\0'; // not needed w/ sprintf_s
    msgpack_object_print_buffer(buffer, sizeof(buffer) - 1, obj);
    EXPECT_STREQ(expected, buffer);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, simple_object_print_buffer_map_str_empty)
{
    const char * mkey = "key";
    char expected[64];
    char buffer[64];
    const size_t mkey_size = strlen(mkey);;
    const unsigned int map_size = 1;

    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_map(&pk, map_size);
    msgpack_pack_str(&pk, mkey_size);
    msgpack_pack_str_body(&pk, mkey, mkey_size);
    msgpack_pack_str(&pk, 0);
    msgpack_pack_str_body(&pk, "", 0);

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_MAP, obj.type);
    EXPECT_EQ(map_size, obj.via.map.size);

    msgpack_object key = obj.via.map.ptr->key;
    msgpack_object val = obj.via.map.ptr->val;
    EXPECT_EQ(MSGPACK_OBJECT_STR, key.type);
    EXPECT_EQ(mkey_size, key.via.str.size);
    EXPECT_EQ(0, memcmp(mkey, key.via.str.ptr, mkey_size));
    EXPECT_EQ(MSGPACK_OBJECT_STR, val.type);
    EXPECT_EQ(0UL, val.via.str.size);

    msgpack_snprintf(expected, sizeof(expected), "{\"%s\"=>\"\"}", mkey);
    expected[sizeof(expected) - 1] = '\0'; // not needed w/ sprintf_s
    msgpack_object_print_buffer(buffer, sizeof(buffer) - 1, obj);
    EXPECT_STREQ(expected, buffer);

    msgpack_zone_destroy(&z);
    msgpack_sbuffer_destroy(&sbuf);
}

TEST(MSGPACKC, unpack_fixstr)
{
    size_t str_size = 7;
    const char buf[] = {
        (char)0xa7, 'f', 'r', 's', 'y', 'u', 'k', 'i'
    };

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
}

TEST(MSGPACKC, unpack_str8)
{
    size_t str_size = 7;
    const char buf[] = {
        (char)0xd9, 7, 'f', 'r', 's', 'y', 'u', 'k', 'i'
    };

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
}

TEST(MSGPACKC, unpack_str16)
{
    size_t str_size = 7;
    const char buf[] = {
        (char)0xda, 0, 7, 'f', 'r', 's', 'y', 'u', 'k', 'i'
    };

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
}

TEST(MSGPACKC, unpack_str32)
{
    size_t str_size = 7;
    const char buf[] = {
        (char)0xdb, 0, 0, 0, 7, 'f', 'r', 's', 'y', 'u', 'k', 'i'
    };

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_EQ(str_size, obj.via.str.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.str.ptr, str_size));

    msgpack_zone_destroy(&z);
}

TEST(MSGPACKC, unpack_bin8)
{
    size_t bin_size = 7;
    const char buf[] = {
        (char)0xc4, 7, 'f', 'r', 's', 'y', 'u', 'k', 'i'
    };

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_BIN, obj.type);
    EXPECT_EQ(bin_size, obj.via.bin.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.bin.ptr, bin_size));

    msgpack_zone_destroy(&z);
}

TEST(MSGPACKC, unpack_bin16)
{
    size_t bin_size = 7;
    const char buf[] = {
        (char)0xc5, 0, 7, 'f', 'r', 's', 'y', 'u', 'k', 'i'
    };

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_BIN, obj.type);
    EXPECT_EQ(bin_size, obj.via.bin.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.bin.ptr, bin_size));

    msgpack_zone_destroy(&z);
}

TEST(MSGPACKC, unpack_bin32)
{
    size_t bin_size = 7;
    const char buf[] = {
        (char)0xc6, 0, 0, 0, 7, 'f', 'r', 's', 'y', 'u', 'k', 'i'
    };

    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_BIN, obj.type);
    EXPECT_EQ(bin_size, obj.via.bin.size);
    EXPECT_EQ(0, memcmp("frsyuki", obj.via.bin.ptr, bin_size));

    msgpack_zone_destroy(&z);
}

TEST(MSGPACKC, unpack_array_uint64)
{
    const char buf[] = {
        (char)0x91, (char)0xcf, (char)0xff, (char)0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    msgpack_zone z;
    msgpack_zone_init(&z, 2048);
    msgpack_object obj;
    msgpack_unpack_return ret;
    ret = msgpack_unpack(buf, sizeof(buf), NULL, &z, &obj);
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);
    EXPECT_EQ(MSGPACK_OBJECT_ARRAY, obj.type);
    EXPECT_EQ(1u, obj.via.array.size);
    EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.via.array.ptr[0].type);
    EXPECT_EQ(0xFFF0000000000001LL, obj.via.array.ptr[0].via.u64);
    msgpack_zone_destroy(&z);
}


TEST(MSGPACKC, vref_buffer_overflow)
{
    msgpack_vrefbuffer vbuf;
    msgpack_vrefbuffer to;
    size_t ref_size = 0;
    size_t chunk_size = std::numeric_limits<size_t>::max();
    EXPECT_FALSE(msgpack_vrefbuffer_init(&vbuf, ref_size, chunk_size));
    EXPECT_EQ(-1, msgpack_vrefbuffer_migrate(&vbuf, &to));
}

TEST(MSGPACKC, object_print_buffer_overflow) {
  msgpack_object obj;
  obj.type = MSGPACK_OBJECT_NIL;
  char buffer[4];

  int ret;
  ret = msgpack_object_print_buffer(buffer, 1, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 2, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 3, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 4, obj);
  EXPECT_EQ(3, ret);
  EXPECT_STREQ("nil", buffer);
}

TEST(MSGPACKC, object_bin_print_buffer_overflow) {
  msgpack_object obj;
  obj.type = MSGPACK_OBJECT_BIN;
  obj.via.bin.ptr = "test";
  obj.via.bin.size = 4;
  char buffer[7];

  int ret;
  ret = msgpack_object_print_buffer(buffer, 1, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 2, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 3, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 4, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 5, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 6, obj);
  EXPECT_EQ(0, ret);
  ret = msgpack_object_print_buffer(buffer, 7, obj);
  EXPECT_EQ(6, ret);
  EXPECT_STREQ("\"test\"", buffer);
}

TEST(MSGPACKC, init_msgpack_obj_nil) {
    msgpack_object obj;
    msgpack_object_init_nil(&obj);
    EXPECT_EQ(MSGPACK_OBJECT_NIL, obj.type);
}

TEST(MSGPACKC, init_msgpack_obj_boolean) {
    msgpack_object obj;
    msgpack_object_init_boolean(&obj, true);
    EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, obj.type);
    EXPECT_EQ(true, obj.via.boolean);
}

TEST(MSGPACKC, init_msgpack_obj_unsigned_integer) {
    msgpack_object obj;
    msgpack_object_init_unsigned_integer(&obj, 123);
    EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.type);
    EXPECT_EQ(static_cast<uint64_t>(123), obj.via.u64);
}

TEST(MSGPACKC, init_msgpack_obj_signed_integer1) {
    msgpack_object obj;
    msgpack_object_init_signed_integer(&obj, -123);
    EXPECT_EQ(MSGPACK_OBJECT_NEGATIVE_INTEGER, obj.type);
    EXPECT_EQ(-123, obj.via.i64);
}

TEST(MSGPACKC, init_msgpack_obj_signed_integer2) {
    msgpack_object obj;
    msgpack_object_init_signed_integer(&obj, 123);
    EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.type);
    EXPECT_EQ(static_cast<uint64_t>(123), obj.via.u64);
}

TEST(MSGPACKC, init_msgpack_obj_float32) {
    msgpack_object obj;
    float val = 1.23f;
    msgpack_object_init_float32(&obj, val);
    EXPECT_EQ(MSGPACK_OBJECT_FLOAT32, obj.type);
    EXPECT_TRUE(fabs(obj.via.f64 - val) <= kEPS);
}

TEST(MSGPACKC, init_msgpack_obj_float64) {
    msgpack_object obj;
    double val = 1.23;
    msgpack_object_init_float64(&obj, val);
    EXPECT_EQ(MSGPACK_OBJECT_FLOAT64, obj.type);
    EXPECT_TRUE(fabs(obj.via.f64 - val) <= kEPS);
}


TEST(MSGPACKC, init_msgpack_obj_string) {
    msgpack_object obj;
    char buffer[] = "test";
    msgpack_object_init_str(&obj, buffer, (uint32_t)strlen(buffer));
    EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
    EXPECT_STREQ(buffer, obj.via.str.ptr);
}

TEST(MSGPACKC, init_msgpack_obj_bin) {
    msgpack_object obj;
    char buffer[] = "test";
    msgpack_object_init_bin(&obj, buffer, (uint32_t)strlen(buffer));
    EXPECT_EQ(MSGPACK_OBJECT_BIN, obj.type);
    EXPECT_STREQ(buffer, obj.via.bin.ptr);
}

TEST(MSGPACKC, init_msgpack_obj_ext) {
    msgpack_object obj;
    char buffer[] = "test";
    msgpack_object_init_ext(&obj, 1, buffer, (uint32_t)strlen(buffer));
    EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
    EXPECT_EQ(1, obj.via.ext.type);
    EXPECT_STREQ(buffer, obj.via.ext.ptr);
}

TEST(MSGPACKC, init_msgpack_obj_array) {
    msgpack_object obj;
    char buffer[][7] = {"test_1", "test_2", "test_3", "test_4"};
    uint32_t buffer_size = 4;
    msgpack_object array[buffer_size];
    for(size_t i = 0; i < buffer_size; i++) {
        msgpack_object_init_str(&array[i], buffer[i], (uint32_t)strlen(buffer[i]));
    }
    msgpack_object_init_array(&obj, array, buffer_size);
    EXPECT_EQ(MSGPACK_OBJECT_ARRAY, obj.type);
    for(size_t i = 0; i < buffer_size; i++) {
        EXPECT_STREQ(buffer[i], obj.via.array.ptr[i].via.str.ptr);
    }
}

TEST(MSGPACKC, init_msgpack_obj_map) {
    msgpack_object obj;
    char key_str[] = "test_key";
    char value_str[] = "test_value";
    msgpack_object key,value;
    msgpack_object_init_str(&key, key_str, (uint32_t)strlen(key_str));
    msgpack_object_init_str(&value, value_str, (uint32_t)strlen(value_str));
    msgpack_object_kv map = { key, value };
    msgpack_object_init_map(&obj, &map, 1);
    EXPECT_EQ(MSGPACK_OBJECT_MAP, obj.type);
    EXPECT_STREQ(key_str, obj.via.map.ptr->key.via.str.ptr);
    EXPECT_STREQ(value_str, obj.via.map.ptr->val.via.str.ptr);
}

/* test for vrefbuffer */
#define GEN_TEST_VREFBUFFER_PREPARE(...)                       \
    msgpack_vrefbuffer vbuf;                                   \
    msgpack_packer pk;                                         \
    const msgpack_iovec *iov;                                  \
    size_t iovcnt, len = 0, i;                                 \
    char buf[1024];                                            \
    msgpack_vrefbuffer_init(&vbuf, 0, 0);                      \
    msgpack_packer_init(&pk, &vbuf, msgpack_vrefbuffer_write); \
    __VA_ARGS__;                                               \
    iov = msgpack_vrefbuffer_vec(&vbuf);                       \
    iovcnt = msgpack_vrefbuffer_veclen(&vbuf);                 \
    for (i = 0; i < iovcnt; i++) {                             \
        memcpy(buf + len, iov[i].iov_base, iov[i].iov_len);    \
        len += iov[i].iov_len;                                 \
    }                                                          \
    msgpack_vrefbuffer_destroy(&vbuf)

#define GEN_TEST_VREFBUFFER_CHECK(...)                \
    msgpack_object obj;                               \
    msgpack_unpack_return ret;                        \
    msgpack_zone z;                                   \
    msgpack_zone_init(&z, 2048);                      \
    ret = msgpack_unpack(buf, len, NULL, &z, &obj);   \
    EXPECT_EQ(MSGPACK_UNPACK_SUCCESS, ret);           \
    __VA_ARGS__;                                      \
    msgpack_zone_destroy(&z)

TEST(buffer, vrefbuffer_uint8)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_uint8(&pk, 32));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.type);
        EXPECT_EQ(32U, obj.via.u64));
}

TEST(buffer, vrefbuffer_int8)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_int8(&pk, -32));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_NEGATIVE_INTEGER, obj.type);
        EXPECT_EQ(-32, obj.via.i64));
}

TEST(buffer, vrefbuffer_float32)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_float(&pk, 1.0));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_FLOAT32, obj.type);
        EXPECT_EQ(1.0, obj.via.f64));
}

TEST(buffer, vrefbuffer_float64)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_double(&pk, 1.0));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_FLOAT64, obj.type);
        EXPECT_EQ(1.0, obj.via.f64));
}

TEST(buffer, vrefbuffer_nil)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_nil(&pk));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_NIL, obj.type));
}

TEST(buffer, vrefbuffer_false)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_false(&pk));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, obj.type);
        EXPECT_FALSE(obj.via.boolean));
}

TEST(buffer, vrefbuffer_true)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_true(&pk));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_BOOLEAN, obj.type);
        EXPECT_TRUE(obj.via.boolean));
}

#define TEST_VBUF_RAW_LEN 30U
char test_vbuf_raw[TEST_VBUF_RAW_LEN] = "frsyuki";

TEST(buffer, vrefbuffer_str)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_str(&pk, TEST_VBUF_RAW_LEN);
        msgpack_pack_str_body(&pk, test_vbuf_raw, TEST_VBUF_RAW_LEN));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_STR, obj.type);
        EXPECT_EQ(TEST_VBUF_RAW_LEN, obj.via.str.size);
        EXPECT_EQ(0, memcmp(test_vbuf_raw, obj.via.str.ptr, 30)));
}

TEST(buffer, vrefbuffer_bin)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_bin(&pk, TEST_VBUF_RAW_LEN);
        msgpack_pack_bin_body(&pk, test_vbuf_raw, TEST_VBUF_RAW_LEN));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(TEST_VBUF_RAW_LEN, obj.via.bin.size);
        EXPECT_EQ(0, memcmp(test_vbuf_raw, obj.via.bin.ptr, TEST_VBUF_RAW_LEN)));
}

TEST(buffer, vrefbuffer_ext)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_ext(&pk, TEST_VBUF_RAW_LEN, 127);
        msgpack_pack_ext_body(&pk, test_vbuf_raw, TEST_VBUF_RAW_LEN));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_EXT, obj.type);
        EXPECT_EQ(TEST_VBUF_RAW_LEN, obj.via.ext.size);
        EXPECT_EQ(0, memcmp(test_vbuf_raw, obj.via.ext.ptr, TEST_VBUF_RAW_LEN)));
}

TEST(buffer, vrefbuffer_array)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_array(&pk, 2);
        msgpack_pack_int(&pk, 3);
        msgpack_pack_int(&pk, 4));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_ARRAY, obj.type);
        EXPECT_EQ(2U, obj.via.array.size);
        EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.via.array.ptr[0].type);
        EXPECT_EQ(3U, obj.via.array.ptr[0].via.u64);
        EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.via.array.ptr[1].type);
        EXPECT_EQ(4U, obj.via.array.ptr[1].via.u64));
}

TEST(buffer, vrefbuffer_map)
{
    GEN_TEST_VREFBUFFER_PREPARE(
        msgpack_pack_map(&pk, 1);
        msgpack_pack_int(&pk, 2);
        msgpack_pack_int(&pk, 3));
    GEN_TEST_VREFBUFFER_CHECK(
        EXPECT_EQ(MSGPACK_OBJECT_MAP, obj.type);
        EXPECT_EQ(1U, obj.via.map.size);
        EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.via.map.ptr[0].key.type);
        EXPECT_EQ(2U, obj.via.map.ptr[0].key.via.u64);
        EXPECT_EQ(MSGPACK_OBJECT_POSITIVE_INTEGER, obj.via.map.ptr[0].val.type);
        EXPECT_EQ(3U, obj.via.map.ptr[0].val.via.u64));
}
