#include <msgpack/fbuffer.h>
#include <msgpack/zbuffer.h>
#include <msgpack/sbuffer.h>
#include <msgpack/vrefbuffer.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif //defined(__GNUC__)

#include <gtest/gtest.h>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif //defined(__GNUC__)

#include <string.h>

#if defined(unix) || defined(__unix) || defined(__linux__) || defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__QNX__) || defined(__QNXTO__) || defined(__HAIKU__)
#define HAVE_SYS_UIO_H 1
#else
#define HAVE_SYS_UIO_H 0
#endif

TEST(buffer, zbuffer_c)
{
    msgpack_zbuffer zbuf;
    EXPECT_TRUE(msgpack_zbuffer_init(&zbuf, 1, MSGPACK_ZBUFFER_INIT_SIZE));
    EXPECT_EQ(0, msgpack_zbuffer_write(&zbuf, "a", 1));
    EXPECT_EQ(0, msgpack_zbuffer_write(&zbuf, "a", 1));
    EXPECT_EQ(0, msgpack_zbuffer_write(&zbuf, "a", 1));
    EXPECT_EQ(0, msgpack_zbuffer_write(&zbuf, "", 0));

    EXPECT_TRUE(msgpack_zbuffer_flush(&zbuf) != NULL);

    msgpack_zbuffer_destroy(&zbuf);
}

TEST(buffer, fbuffer_c)
{
#if defined(_MSC_VER)
    FILE* file;
    tmpfile_s(&file);
#else  // defined(_MSC_VER)
    FILE* file = tmpfile();
#endif // defined(_MSC_VER)

    void* fbuf = (void*)file;

    EXPECT_TRUE( file != NULL );
    EXPECT_EQ(0, msgpack_fbuffer_write(fbuf, "a", 1));
    EXPECT_EQ(0, msgpack_fbuffer_write(fbuf, "a", 1));
    EXPECT_EQ(0, msgpack_fbuffer_write(fbuf, "a", 1));

    fflush(file);
    rewind(file);
    for (size_t i=0; i < 3; ++i) {
        int ch = fgetc(file);
        EXPECT_TRUE(ch != EOF);
        EXPECT_EQ('a', (char) ch);
    }
    EXPECT_EQ(EOF, fgetc(file));
    fclose(file);
}

TEST(buffer, sbuffer_c)
{
    msgpack_sbuffer *sbuf;
    char *data;

    sbuf = msgpack_sbuffer_new();
    EXPECT_TRUE(sbuf != NULL);
    EXPECT_EQ(0, msgpack_sbuffer_write(sbuf, "a", 1));
    EXPECT_EQ(0, msgpack_sbuffer_write(sbuf, "b", 1));
    EXPECT_EQ(0, msgpack_sbuffer_write(sbuf, "c", 1));
    EXPECT_EQ(0, msgpack_sbuffer_write(sbuf, "", 0));
    EXPECT_EQ(3U, sbuf->size);
    EXPECT_EQ(0, memcmp(sbuf->data, "abc", 3));
    data = msgpack_sbuffer_release(sbuf);
    EXPECT_EQ(0, memcmp(data, "abc", 3));
    EXPECT_EQ(0U, sbuf->size);
    EXPECT_TRUE(sbuf->data == NULL);

    free(data);
    msgpack_sbuffer_free(sbuf);
}

TEST(buffer, vrefbuffer_c)
{
    const char *raw = "I was about to sail away in a junk,"
                      "When suddenly I heard"
                      "The sound of stamping and singing on the bank--"
                      "It was you and your friends come to bid me farewell."
                      "The Peach Flower Lake is a thousand fathoms deep,"
                      "But it cannot compare, O Wang Lun,"
                      "With the depth of your love for me.";
    const size_t rawlen = strlen(raw);
    msgpack_vrefbuffer *vbuf;
    const int ref_size = 24, chunk_size = 128;
    size_t slices[] = {0, 9, 10,
                    MSGPACK_VREFBUFFER_REF_SIZE,
                    MSGPACK_VREFBUFFER_REF_SIZE + 1,
                    ref_size, chunk_size + 1};
    size_t iovcnt;
    const msgpack_iovec *iov;
    size_t len = 0, i;
    char *buf;

    vbuf = msgpack_vrefbuffer_new(ref_size, 0);
    for (i = 0; i < sizeof(slices) / sizeof(slices[0]); i++) {
        msgpack_vrefbuffer_write(vbuf, raw + len, slices[i]);
        len += slices[i];
    }
    EXPECT_LT(len, rawlen);
    iov = msgpack_vrefbuffer_vec(vbuf);
    iovcnt = msgpack_vrefbuffer_veclen(vbuf);

    buf = (char *)malloc(rawlen);
#if HAVE_SYS_UIO_H
    {
        int fd;
        char filename[] = "/tmp/mp.XXXXXX";

        fd = mkstemp(filename);
        EXPECT_LT(0, fd);
        writev(fd, iov, (int)iovcnt);
        len = (size_t)lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        read(fd, buf, len);
        EXPECT_EQ(0, memcmp(buf, raw, len));
        close(fd);
        unlink(filename);
    }
#else
    {
        len = 0;
        for (i = 0; i < iovcnt; i++)
        {
            EXPECT_LT(len, rawlen);
            memcpy(buf + len, iov[i].iov_base, iov[i].iov_len);
            len += iov[i].iov_len;
        }
        EXPECT_EQ(0, memcmp(buf, raw, len));
    }
#endif
    free(buf);
    msgpack_vrefbuffer_free(vbuf);
}
