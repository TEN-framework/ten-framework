#include <msgpack.h>
#include <stdio.h>
#include <assert.h>

#define UNPACKED_BUFFER_SIZE 2048

void prepare(msgpack_sbuffer* sbuf) {
    msgpack_packer pk;

    msgpack_packer_init(&pk, sbuf, msgpack_sbuffer_write);
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
}

void unpack(char const* buf, size_t len) {
    /* buf is allocated by client. */
    msgpack_unpacked result;
    size_t off = 0;
    msgpack_unpack_return ret;
    int i = 0;
    char unpacked_buffer[UNPACKED_BUFFER_SIZE];
    msgpack_unpacked_init(&result);
    ret = msgpack_unpack_next(&result, buf, len, &off);
    while (ret == MSGPACK_UNPACK_SUCCESS) {
        msgpack_object obj = result.data;

        /* Use obj. */
        printf("Object no %d:\n", ++i);
        msgpack_object_print(stdout, obj);
        printf("\n");
        msgpack_object_print_buffer(unpacked_buffer, UNPACKED_BUFFER_SIZE, obj);
        printf("%s\n", unpacked_buffer);
        /* If you want to allocate something on the zone, you can use zone. */
        /* msgpack_zone* zone = result.zone; */
        /* The lifetime of the obj and the zone,  */

        ret = msgpack_unpack_next(&result, buf, len, &off);
    }
    msgpack_unpacked_destroy(&result);

    if (ret == MSGPACK_UNPACK_CONTINUE) {
        printf("All msgpack_object in the buffer is consumed.\n");
    }
    else if (ret == MSGPACK_UNPACK_PARSE_ERROR) {
        printf("The data in the buf is invalid format.\n");
    }
}

int main(void) {
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);

    prepare(&sbuf);
    unpack(sbuf.data, sbuf.size);

    msgpack_sbuffer_destroy(&sbuf);
    return 0;
}

/* Output */

/*
Object no 1:
[1, true, "example"]
Object no 2:
"second"
Object no 3:
[42, false]
All msgpack_object in the buffer is consumed.
*/
