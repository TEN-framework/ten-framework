/* gcc boundary.c -o boundary -Wconversion -Wpointer-sign */
#include <msgpack.h>
#include <stdio.h>
#include <assert.h>

static inline unsigned char atohex(char a)
{
    int x;
    if (a >= 'a') {
        x = a - 'a' + 10;
    } else if (a >= 'A') {
        x = a - 'A' + 10;
    } else {
        x = a - '0';
    }
    assert(x >= 0 && x < 16);
    return (unsigned char)x;
}

// Return 0 if equal
static inline int bytesncmp(char *data, const char *bytes, size_t len)
{
    size_t n = len >> 1;
    size_t i = 0;
    int diff;
    for (; i < n; i++) {
        diff = (unsigned char)data[i] - (atohex(bytes[2 * i]) << 4) - atohex(bytes[2 * i + 1]);
        if (diff != 0) {
            return diff;
        }
    }
    return 0;
}

int main()
{
    msgpack_sbuffer sbuf;
    msgpack_packer *x;
    size_t offset = 0;
    char data[65536];
    msgpack_timestamp ts[] = {
        { 0xFFFFFFFF, 0 },
        { 0x100000000, 0 },
        { 0x3FFFFFFFF, 0 },
        { 0x400000000, 0 },
        { INT64_MAX, UINT32_MAX }
    };

#define check_sbuffer(b)                                     \
    do {                                                     \
        size_t len = strlen(#b);                             \
        assert((sbuf.size - offset) * 2 == len);             \
        assert(bytesncmp(sbuf.data + offset, #b, len) == 0); \
        offset = sbuf.size;                                  \
    } while (0)

    msgpack_sbuffer_init(&sbuf);
    x = msgpack_packer_new(&sbuf, msgpack_sbuffer_write);

    msgpack_pack_fix_uint8(x, 0); check_sbuffer(cc00);          /* cc 00 */
    msgpack_pack_fix_uint8(x, 0xFF); check_sbuffer(ccff);       /* cc ff */
    msgpack_pack_fix_uint16(x, 0); check_sbuffer(cd0000);       /* cd 00 00 */
    msgpack_pack_fix_uint16(x, 0xFFFF); check_sbuffer(cdffff);  /* cd ff ff */
    msgpack_pack_fix_uint32(x, 0); check_sbuffer(ce00000000);   /* ce 00 00 00 00 */
    msgpack_pack_fix_uint32(x, 0xFFFFFFFF); check_sbuffer(ceffffffff);  /* ce ff ff ff ff */
    msgpack_pack_fix_uint64(x, 0); check_sbuffer(cf0000000000000000);   /* cf 00 00 00 00 00 00 00 00 */
    msgpack_pack_fix_uint64(x, 0xFFFFFFFFFFFFFFFF); check_sbuffer(cfffffffffffffffff);  /* cf ff ff ff ff ff ff ff ff */

    msgpack_pack_uint8(x, 0); check_sbuffer(00);            /* 00 */
    msgpack_pack_uint8(x, 0x7F); check_sbuffer(7f);         /* 7f */
    msgpack_pack_uint8(x, 0x80); check_sbuffer(cc80);       /* cc 80 */
    msgpack_pack_uint8(x, 0xFF); check_sbuffer(ccff);       /* cc ff */

    msgpack_pack_uint16(x, 0); check_sbuffer(00);           /* 00 */
    msgpack_pack_uint16(x, 0x7F); check_sbuffer(7f);        /* 7f */
    msgpack_pack_uint16(x, 0x80); check_sbuffer(cc80);      /* cc 80 */
    msgpack_pack_uint16(x, 0xFF); check_sbuffer(ccff);      /* cc ff */
    msgpack_pack_uint16(x, 0x100); check_sbuffer(cd0100);   /* cd 01 00 */
    msgpack_pack_uint16(x, 0xFFFF); check_sbuffer(cdffff);  /* cd ff ff */

    msgpack_pack_uint32(x, 0); check_sbuffer(00);           /* 00 */
    msgpack_pack_uint32(x, 0x7F); check_sbuffer(7f);        /* 7f */
    msgpack_pack_uint32(x, 0x80); check_sbuffer(cc80);      /* cc 80 */
    msgpack_pack_uint32(x, 0xFF); check_sbuffer(ccff);      /* cc ff */
    msgpack_pack_uint32(x, 0x100); check_sbuffer(cd0100);   /* cd 01 00 */
    msgpack_pack_uint32(x, 0xFFFF); check_sbuffer(cdffff);  /* cd ff ff */
    msgpack_pack_uint32(x, 0x10000); check_sbuffer(ce00010000);     /* ce 00 01 00 00 */
    msgpack_pack_uint32(x, 0xFFFFFFFF); check_sbuffer(ceffffffff);  /* ce ff ff ff ff */

    msgpack_pack_uint64(x, 0); check_sbuffer(00);           /* 00 */
    msgpack_pack_uint64(x, 0x7F); check_sbuffer(7f);        /* 7f */
    msgpack_pack_uint64(x, 0x80); check_sbuffer(cc80);      /* cc 80 */
    msgpack_pack_uint64(x, 0xFF); check_sbuffer(ccff);      /* cc ff */
    msgpack_pack_uint64(x, 0x100); check_sbuffer(cd0100);   /* cd 01 00 */
    msgpack_pack_uint64(x, 0xFFFF); check_sbuffer(cdffff);  /* cd ff ff */
    msgpack_pack_uint64(x, 0x10000); check_sbuffer(ce00010000);     /* ce 00 01 00 00 */
    msgpack_pack_uint64(x, 0xFFFFFFFF); check_sbuffer(ceffffffff);  /* ce ff ff ff ff */
    msgpack_pack_uint64(x, 0x100000000); check_sbuffer(cf0000000100000000);         /* cf 00 00 00 01 00 00 00 00 */
    msgpack_pack_uint64(x, 0xFFFFFFFFFFFFFFFF); check_sbuffer(cfffffffffffffffff);  /* cf ff ff ff ff ff ff ff ff */

    msgpack_pack_fix_int8(x, 0x7F); check_sbuffer(d07f);            /* d0 7f */
    msgpack_pack_fix_int8(x, -0x7F-1); check_sbuffer(d080);         /* d0 80 */
    msgpack_pack_fix_int16(x, 0x7FFF); check_sbuffer(d17fff);       /* d1 7f ff */
    msgpack_pack_fix_int16(x, -0x7FFF-1); check_sbuffer(d18000);    /* d1 80 00 */
    msgpack_pack_fix_int32(x, 0x7FFFFFFF); check_sbuffer(d27fffffff);       /* d2 7f ff ff ff */
    msgpack_pack_fix_int32(x, -0x7FFFFFFF-1); check_sbuffer(d280000000);    /* d2 80 00 00 00 */
    msgpack_pack_fix_int64(x, 0x7FFFFFFFFFFFFFFF); check_sbuffer(d37fffffffffffffff);       /* d3 7f ff ff ff ff ff ff ff */
    msgpack_pack_fix_int64(x, -0x7FFFFFFFFFFFFFFF-1); check_sbuffer(d38000000000000000);    /* d3 80 00 00 00 00 00 00 00 */

    msgpack_pack_int8(x, -0x7F-1); check_sbuffer(d080);         /* d0 80 */
    msgpack_pack_int8(x, -0x21); check_sbuffer(d0df);           /* d0 df */
    msgpack_pack_int8(x, -0x20); check_sbuffer(e0);             /* e0 */
    msgpack_pack_int8(x, -1); check_sbuffer(ff);                /* ff */
    msgpack_pack_int8(x, 0); check_sbuffer(00);                 /* 00 */
    msgpack_pack_int8(x, 0x7F); check_sbuffer(7f);              /* 7f */

    msgpack_pack_int16(x, -0x7FFF-1); check_sbuffer(d18000);    /* d1 80 00 */
    msgpack_pack_int16(x, -0x81); check_sbuffer(d1ff7f);        /* d1 ff 7f */
    msgpack_pack_int16(x, -0x80); check_sbuffer(d080);          /* d0 80 */
    msgpack_pack_int16(x, -0x21); check_sbuffer(d0df);          /* d0 df */
    msgpack_pack_int16(x, -0x20); check_sbuffer(e0);            /* e0 */
    msgpack_pack_int16(x, -0x1); check_sbuffer(ff);             /* ff */
    msgpack_pack_int16(x, 0); check_sbuffer(00);                /* 00 */
    msgpack_pack_int16(x, 0x7F); check_sbuffer(7f);             /* 7f */
    msgpack_pack_int16(x, 0x80); check_sbuffer(cc80);           /* cc 80 */
    msgpack_pack_int16(x, 0xFF); check_sbuffer(ccff);           /* cc ff */
    msgpack_pack_int16(x, 0x100); check_sbuffer(cd0100);        /* cd 01 00 */
    msgpack_pack_int16(x, 0x7FFF); check_sbuffer(cd7fff);       /* cd 7f ff */

    msgpack_pack_int32(x, -0x7FFFFFFF-1); check_sbuffer(d280000000);    /* d2 80 00 00 00 */
    msgpack_pack_int32(x, -0x8001); check_sbuffer(d2ffff7fff);          /* d2 ff ff 7f ff */
    msgpack_pack_int32(x, -0x8000); check_sbuffer(d18000);              /* d1 80 00 */
    msgpack_pack_int32(x, -0x81); check_sbuffer(d1ff7f);                /* d1 ff 7f */
    msgpack_pack_int32(x, -0x80); check_sbuffer(d080);                  /* d0 80 */
    msgpack_pack_int32(x, -0x21); check_sbuffer(d0df);                  /* d0 df */
    msgpack_pack_int32(x, -0x20); check_sbuffer(e0);                    /* e0 */
    msgpack_pack_int32(x, -0x1); check_sbuffer(ff);                     /* ff */
    msgpack_pack_int32(x, 0); check_sbuffer(00);                        /* 00 */
    msgpack_pack_int32(x, 0x7F); check_sbuffer(7f);                     /* 7f */
    msgpack_pack_int32(x, 0x80); check_sbuffer(cc80);                   /* cc 80 */
    msgpack_pack_int32(x, 0xFF); check_sbuffer(ccff);                   /* cc ff */
    msgpack_pack_int32(x, 0x100); check_sbuffer(cd0100);                /* cd 01 00 */
    msgpack_pack_int32(x, 0xFFFF); check_sbuffer(cdffff);               /* cd ff ff */
    msgpack_pack_int32(x, 0x10000); check_sbuffer(ce00010000);          /* ce 00 01 00 00 */
    msgpack_pack_int32(x, 0x7FFFFFFF); check_sbuffer(ce7fffffff);       /* ce 7f ff ff ff */

    msgpack_pack_int64(x, -0x7FFFFFFFFFFFFFFF-1); check_sbuffer(d38000000000000000);    /* d3 80 00 00 00 00 00 00 00 */
    msgpack_pack_int64(x, -((1LL<<31)+1)); check_sbuffer(d3ffffffff7fffffff);   /* d3 ff ff ff ff 7f ff ff ff */
    msgpack_pack_int64(x, -(1LL<<31)); check_sbuffer(d280000000);               /* d2 80 00 00 00 */
    msgpack_pack_int64(x, -0x8001); check_sbuffer(d2ffff7fff);                  /* d2 ff ff 7f ff */
    msgpack_pack_int64(x, -0x8000); check_sbuffer(d18000);                      /* d1 80 00 */
    msgpack_pack_int64(x, -0x81); check_sbuffer(d1ff7f);                        /* d1 ff 7f */
    msgpack_pack_int64(x, -0x80); check_sbuffer(d080);                          /* d0 80 */
    msgpack_pack_int64(x, -0x21); check_sbuffer(d0df);                          /* d0 df */
    msgpack_pack_int64(x, -0x20); check_sbuffer(e0);                            /* e0 */
    msgpack_pack_int64(x, -0x1); check_sbuffer(ff);                             /* ff */
    msgpack_pack_int64(x, 0); check_sbuffer(00);                                /* 00 */
    msgpack_pack_int64(x, 0x7F); check_sbuffer(7f);                             /* 7f */
    msgpack_pack_int64(x, 0x80); check_sbuffer(cc80);                           /* cc 80 */
    msgpack_pack_int64(x, 0xFF); check_sbuffer(ccff);                           /* cc ff */
    msgpack_pack_int64(x, 0x100); check_sbuffer(cd0100);                        /* cd 01 00 */
    msgpack_pack_int64(x, 0xFFFF); check_sbuffer(cdffff);                       /* cd ff ff */
    msgpack_pack_int64(x, 0x10000); check_sbuffer(ce00010000);                  /* ce 00 01 00 00 */
    msgpack_pack_int64(x, 0xFFFFFFFF); check_sbuffer(ceffffffff);               /* ce ff ff ff ff */
    msgpack_pack_int64(x, 0x100000000); check_sbuffer(cf0000000100000000);      /* cf 00 00 00 01 00 00 00 00 */
    msgpack_pack_int64(x, 0x7FFFFFFFFFFFFFFF); check_sbuffer(cf7fffffffffffffff);   /* cf 7f ff ff ff ff ff ff ff */

    msgpack_pack_nil(x);    check_sbuffer(c0);          /* c0 */
    msgpack_pack_false(x);  check_sbuffer(c2);          /* c2 */
    msgpack_pack_true(x);   check_sbuffer(c3);          /* c3 */

    msgpack_pack_float(x, 1.0); check_sbuffer(ca3f800000);          /* ca 3f 80 00 00 */
    msgpack_pack_double(x, 1.0); check_sbuffer(cb3ff0000000000000); /* cb 3f f0 00 00 00 00 00 00 */

    msgpack_pack_unsigned_char(x, UINT8_MAX);       /* same as msgpack_pack_uint8() */
    msgpack_pack_unsigned_short(x, (unsigned short)UINT64_MAX);
    msgpack_pack_unsigned_int(x, (unsigned int)UINT64_MAX);
    msgpack_pack_unsigned_long(x, (unsigned long)UINT64_MAX);
    msgpack_pack_unsigned_long_long(x, (unsigned long long)UINT64_MAX);

    msgpack_pack_signed_char(x, INT8_MAX);          /* same as msgpack_pack_int8() */

#define check_sbuffer_n(b)                                   \
    do {                                                     \
        size_t len = strlen(#b);                             \
        assert(bytesncmp(sbuf.data + offset, #b, len) == 0); \
        offset = sbuf.size;                                  \
    } while (0)

#define fill_str(n) msgpack_pack_str_body(x, data, n)

    offset = sbuf.size;
    msgpack_pack_str(x, 0); /* "" */ check_sbuffer(a0); /* a0 */
    msgpack_pack_str(x, 31);
    fill_str(31); check_sbuffer_n(bf);                  /* bf ... */
    msgpack_pack_str(x, 32);
    fill_str(32); check_sbuffer_n(d920);                /* d9 20 ... */
    msgpack_pack_str(x, 255);
    fill_str(255); check_sbuffer_n(d9ff);               /* d9 ff ... */
    msgpack_pack_str(x, 256);
    fill_str(256); check_sbuffer_n(da0100);             /* da 01 00 ... */
    msgpack_pack_str(x, 65535);
    fill_str(65535); check_sbuffer_n(daffff);           /* da ff ff ... */
    msgpack_pack_str(x, 65536);
    fill_str(65536); check_sbuffer_n(db00010000);       /* db 00 01 00 00 ... */

#define fill_map(n)                                             \
    do {                                                        \
        size_t i = 0;                                           \
        for (; i < n * 2; i++) { msgpack_pack_int8(x, 0x1); }   \
    } while (0);

    msgpack_pack_map(x, 0); /* {} */ check_sbuffer(80); /* 80 */
    msgpack_pack_map(x, 1);
    fill_map(1); check_sbuffer_n(81);                   /* 81 ... */
    msgpack_pack_map(x, 15);
    fill_map(15); check_sbuffer_n(8f);                  /* 8f ... */
    msgpack_pack_map(x, 16);
    fill_map(16); check_sbuffer_n(de0010);              /* de 00 10 ... */
    msgpack_pack_map(x, 65535);
    fill_map(65535); check_sbuffer_n(deffff);           /* de ff ff ... */
    msgpack_pack_map(x, 65536);
    fill_map(65536); check_sbuffer_n(df00010000);       /* df 00 01 00 00 ... */

#define fill_array(n)                                       \
    do {                                                    \
        size_t i = 0;                                       \
        for (; i < n; i++) { msgpack_pack_int8(x, 0x1); }   \
    } while (0);

    msgpack_pack_array(x, 0); /* [] */ check_sbuffer(90);   /* 90 */
    msgpack_pack_array(x, 1);
    fill_array(1); check_sbuffer_n(91);                 /* 91 ... */
    msgpack_pack_array(x, 15);
    fill_array(15); check_sbuffer_n(9f);                /* 9f ... */
    msgpack_pack_array(x, 16);
    fill_array(16); check_sbuffer_n(dc0010);            /* dc 00 10 ... */
    msgpack_pack_array(x, 65535);
    fill_array(65535); check_sbuffer_n(dcffff);         /* dc ff ff ... */
    msgpack_pack_array(x, 65536);
    fill_array(65536); check_sbuffer_n(dd00010000);     /* dd 00 01 00 00 ... */

#define fill_bin(n) msgpack_pack_bin_body(x, data, n)

    msgpack_pack_bin(x, 0); check_sbuffer(c400);        /* c4 00 */
    msgpack_pack_bin(x, 1);
    fill_bin(1); check_sbuffer_n(c401);                 /* c4 01 ... */
    msgpack_pack_bin(x, 255);
    fill_bin(255); check_sbuffer_n(c4ff);               /* c4 ff ... */
    msgpack_pack_bin(x, 256);
    fill_bin(256); check_sbuffer_n(c50100);             /* c5 01 00 ... */
    msgpack_pack_bin(x, 65535);
    fill_bin(65535); check_sbuffer_n(c5ffff);           /* c5 ff ff ... */
    msgpack_pack_bin(x, 65536);
    fill_bin(65536); check_sbuffer_n(c600010000);       /* c6 00 01 00 00 ... */

#define fill_ext(n) msgpack_pack_ext_body(x, data, n)

    msgpack_pack_ext(x, 1, 0x7F);
    fill_ext(1); check_sbuffer_n(d47f);                 /* d4 7f ... */
    msgpack_pack_ext(x, 2, 0x7F);
    fill_ext(2); check_sbuffer_n(d57f);                 /* d5 7f ... */
    msgpack_pack_ext(x, 4, 0x7F);
    fill_ext(4); check_sbuffer_n(d67f);                 /* d6 7f ... */
    msgpack_pack_ext(x, 8, 0x7F);
    fill_ext(8); check_sbuffer_n(d77f);                 /* d7 7f ... */
    msgpack_pack_ext(x, 16, 0x7F);
    fill_ext(16); check_sbuffer_n(d87f);                /* d8 7f ... */

    msgpack_pack_ext(x, 0, 0x7F); check_sbuffer(c7007f); /* c7 00 7f */
    msgpack_pack_ext(x, 3, 0x7F);
    fill_ext(3); check_sbuffer_n(c7037f);               /* c7 03 7f */
    msgpack_pack_ext(x, 5, 0x7F);
    fill_ext(5); check_sbuffer_n(c7057f);               /* c7 05 7f */
    msgpack_pack_ext(x, 17, 0x7F);
    fill_ext(17); check_sbuffer_n(c7117f);              /* c7 11 7f */
    msgpack_pack_ext(x, 255, 0x7F);
    fill_ext(255); check_sbuffer_n(c7ff7f);             /* c7 ff 7f ... */
    msgpack_pack_ext(x, 256, 0x7F);
    fill_ext(256); check_sbuffer_n(c801007f);           /* c8 01 00 7f ... */
    msgpack_pack_ext(x, 65535, 0x7F);
    fill_ext(65535); check_sbuffer_n(c8ffff7f);         /* c8 ff ff 7f ... */
    msgpack_pack_ext(x, 65536, 0x7F);
    fill_ext(65536); check_sbuffer_n(c9000100007f);     /* c9 00 01 00 00 7f ... */

    msgpack_pack_timestamp(x, ts); check_sbuffer(d6ffffffffff); /* d6 ff ff ff ff ff */
    msgpack_pack_timestamp(x, ts + 1); check_sbuffer(d7ff0000000100000000); /* d7 ff 00 00 00 01 00 00 00 00 */
    msgpack_pack_timestamp(x, ts + 2); check_sbuffer(d7ff00000003ffffffff); /* d7 ff 00 00 00 03 ff ff ff ff */
    msgpack_pack_timestamp(x, ts + 3); check_sbuffer(c70cff000000000000000400000000);   /* c7 0c ff 00 00 00 00 00 00 00 04 00 00 00 00 */
    msgpack_pack_timestamp(x, ts + 4); check_sbuffer(c70cffffffffff7fffffffffffffff);   /* c7 0c ff ff ff ff ff 7f ff ff ff ff ff ff ff */

    msgpack_sbuffer_destroy(&sbuf);
    msgpack_packer_free(x);

    return 0;
}
