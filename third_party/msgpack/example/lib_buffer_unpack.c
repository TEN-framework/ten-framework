#include <msgpack.h>
#include <stdio.h>
#include <assert.h>


typedef struct receiver {
    msgpack_sbuffer sbuf;
    size_t rest;
} receiver;

void receiver_init(receiver *r) {
    msgpack_packer pk;

    msgpack_sbuffer_init(&r->sbuf);
    msgpack_packer_init(&pk, &r->sbuf, msgpack_sbuffer_write);
    /* 1st object */
    msgpack_pack_array(&pk, 3);
    msgpack_pack_int(&pk, 1);
    msgpack_pack_true(&pk);
    msgpack_pack_str(&pk, 7);
    msgpack_pack_str_body(&pk, "example", 7);
    /* 2nd object */
    msgpack_pack_str(&pk, 6);
    msgpack_pack_str_body(&pk, "second", 6);
    /* 3rd object */
    msgpack_pack_array(&pk, 2);
    msgpack_pack_int(&pk, 42);
    msgpack_pack_false(&pk);
    r->rest = r->sbuf.size;
}

size_t receiver_recv(receiver *r, char* buf, size_t try_size) {
    size_t off = r->sbuf.size - r->rest;

    size_t actual_size = try_size;
    if (actual_size > r->rest) actual_size = r->rest;

    memcpy(buf, r->sbuf.data + off, actual_size);
    r->rest -= actual_size;

    return actual_size;
}

size_t receiver_to_unpacker(receiver* r, size_t request_size,
        msgpack_unpacker *unpacker)
{
    size_t recv_len;
    // make sure there's enough room, or expand the unpacker accordingly
    if (msgpack_unpacker_buffer_capacity(unpacker) < request_size) {
        msgpack_unpacker_reserve_buffer(unpacker, request_size);
        assert(msgpack_unpacker_buffer_capacity(unpacker) >= request_size);
    }
    recv_len = receiver_recv(r, msgpack_unpacker_buffer(unpacker),
                             request_size);
    msgpack_unpacker_buffer_consumed(unpacker, recv_len);
    return recv_len;
}

#define EACH_RECV_SIZE 4

void unpack(receiver* r) {
    /* buf is allocated by unpacker. */
    msgpack_unpacker* unp = msgpack_unpacker_new(100);
    msgpack_unpacked result;
    msgpack_unpack_return ret;
    size_t recv_len;
    int recv_count = 0;
    int i = 0;

    msgpack_unpacked_init(&result);
    while (true) {
        recv_len = receiver_to_unpacker(r, EACH_RECV_SIZE, unp);
        if (recv_len == 0) break; // (reached end of input)
#if defined(_MSC_VER) || defined(__MINGW32__)
        printf("receive count: %d %Id bytes received.\n", recv_count++, recv_len);
#else // defined(_MSC_VER) || defined(__MINGW32__)
        printf("receive count: %d %zd bytes received.\n", recv_count++, recv_len);
#endif // defined(_MSC_VER) || defined(__MINGW32__)
        ret = msgpack_unpacker_next(unp, &result);
        while (ret == MSGPACK_UNPACK_SUCCESS) {
            msgpack_object obj = result.data;

            /* Use obj. */
            printf("Object no %d:\n", ++i);
            msgpack_object_print(stdout, obj);
            printf("\n");
            /* If you want to allocate something on the zone, you can use zone. */
            /* msgpack_zone* zone = result.zone; */
            /* The lifetime of the obj and the zone,  */

            ret = msgpack_unpacker_next(unp, &result);
        }
        if (ret == MSGPACK_UNPACK_PARSE_ERROR) {
            printf("The data in the buf is invalid format.\n");
            msgpack_unpacked_destroy(&result);
            return;
        }
    }
    msgpack_unpacked_destroy(&result);
    msgpack_unpacker_free(unp);
}

int main(void) {
    receiver r;
    receiver_init(&r);

    unpack(&r);

    return 0;
}

/* Output */

/*
receive count: 0 4 bytes received.
receive count: 1 4 bytes received.
receive count: 2 4 bytes received.
Object no 1:
[1, true, "example"]
receive count: 3 4 bytes received.
receive count: 4 4 bytes received.
Object no 2:
"second"
receive count: 5 1 bytes received.
Object no 3:
[42, false]
*/
