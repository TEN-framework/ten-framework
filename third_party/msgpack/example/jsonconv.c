#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <msgpack.h>
#include <cjson/cJSON.h>

#if defined(_MSC_VER)
#if _MSC_VER >= 1800
#include <inttypes.h>
#else
#define PRIu64 "I64u"
#define PRIi64 "I64i"
#define PRIi8 "i"
#endif
#else
#include <inttypes.h>
#endif

#if defined(_KERNEL_MODE)
#  undef  snprintf
#  define snprintf _snprintf
#endif

#define DEBUG(...) printf(__VA_ARGS__)

static char *format_string(const char *input)
{
    const char *inptr;
    char *output;
    char *outptr;
    size_t output_length = 0;
    /* numbers of additional characters*/
    size_t escape_characters = 0;

    if (input == NULL) {
        return NULL;
    }

    for (inptr = input; *inptr; inptr++) {
        switch (*inptr) {
        case '\"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            /* one character escape sequence */
            escape_characters++;
            break;
        default:
            break;
        }
    }
    output_length = (size_t)(inptr - input) + escape_characters;

    output = (char *)malloc(output_length + 1);
    if (output == NULL) {
        return NULL;
    }

    /* no add characters*/
    if (escape_characters == 0) {
        memcpy(output, input, output_length);
        output[output_length] = '\0';
        return output;
    }

    outptr = output;
    /* copy string */
    for (inptr = input; *inptr != '\0'; (void)inptr++, outptr++) {
        if ((*inptr > 31) && (*inptr != '\"') && (*inptr != '\\')) {
            /* normal character, copy */
            *outptr = *inptr;
        } else {
            /* character needs to be escaped */
            *outptr++ = '\\';
            switch (*inptr)
            {
            case '\\':
                *outptr = '\\';
                break;
            case '\"':
                *outptr = '\"';
                break;
            case '\b':
                *outptr = 'b';
                break;
            case '\f':
                *outptr = 'f';
                break;
            case '\n':
                *outptr = 'n';
                break;
            case '\r':
                *outptr = 'r';
                break;
            case '\t':
                *outptr = 't';
                break;
            default:
                break;
            }
        }
    }

    output[output_length] = '\0';
    return output;
}

/*
 * Pack cJSON object.
 * return 0 success, others failed
 */
static int parse_cjson_object(msgpack_packer *pk, cJSON *node)
{
    int ret, sz, i;
    cJSON *child;
    char *strvalue;

    if (node == NULL) {
        return -1;
    }

    switch (node->type & 0xFF) {
    case cJSON_Invalid:
        return -1;

    case cJSON_False:
        return msgpack_pack_false(pk);

    case cJSON_True:
        return msgpack_pack_true(pk);

    case cJSON_NULL:
        return msgpack_pack_nil(pk);

    case cJSON_String:
        strvalue = format_string(node->valuestring);
        if (strvalue != NULL) {
            ret = msgpack_pack_str_with_body(pk, strvalue, strlen(strvalue));
            free(strvalue);
            return ret;
        } else {
            return -1;
        }

    case cJSON_Number:
        if (isnan(node->valuedouble) || isinf(node->valuedouble)) {
            ret = msgpack_pack_nil(pk);
        } else if (node->valuedouble == node->valueint) {
            ret = msgpack_pack_int(pk, node->valueint);
        } else {
            ret = msgpack_pack_double(pk, node->valuedouble);
        }
        return ret;

    case cJSON_Array:
        sz = cJSON_GetArraySize(node);
        if (msgpack_pack_array(pk, sz) != 0) {
            return -1;
        }
        for (i = 0; i < sz; i++) {
            if (parse_cjson_object(pk, cJSON_GetArrayItem(node, i)) != 0) {
                return -1;
            }
        }
        return 0;

    case cJSON_Object:
        sz = cJSON_GetArraySize(node);
        if (msgpack_pack_map(pk, sz) != 0) {
            return -1;
        }
        for (i = 0; i < sz; i++) {
            child = cJSON_GetArrayItem(node, i);
            strvalue = format_string(child->string);
            if (strvalue == NULL) {
                return -1;
            }

            if (msgpack_pack_str_with_body(pk, strvalue, strlen(strvalue)) != 0) {
                free(strvalue);
                return -1;
            }
            free(strvalue);
            if (parse_cjson_object(pk, child) != 0) {
                return -1;
            }
        }
        return 0;

    default:
        DEBUG("unknown type.\n");
        return -1;
    }
    return 0;
}

/*
 * Pack json string to msgpack format data.
 * return 0 success, -1 failed
 */
int msgpack_pack_jsonstr(msgpack_packer *pk, const char *jsonstr)
{
    int status;
    cJSON *node;
    const char *end = NULL;

    if (pk == NULL || jsonstr == NULL) {
        return -1;
    }

    node = cJSON_ParseWithOpts(jsonstr, &end, 1);
    if (node == NULL) {
        DEBUG("parse error: unexpected string `%s`\n", end);
        return -1;
    }
    status = parse_cjson_object(pk, node);
    cJSON_Delete(node);

    return status;
}

static int bytes_contain_zero(const msgpack_object_bin *bin)
{
    size_t i;
    for (i = 0; i < bin->size; i++) {
        if (bin->ptr[i] == 0) {
            return 1;
        }
    }
    return 0;
}

#define PRINT_JSONSTR_CALL(ret, func, aux_buffer, aux_buffer_size, ...) \
    ret = func(aux_buffer, aux_buffer_size, __VA_ARGS__);               \
    if (ret <= 0)                                                       \
        return ret;                                                     \
    if (ret > aux_buffer_size)                                          \
        return 0;                                                       \
    aux_buffer = aux_buffer + ret;                                      \
    aux_buffer_size = aux_buffer_size - ret

/*
 * Convert msgpack format data to json string.
 * return >0: success, 0: length of buffer not enough, -1: failed
 */
size_t msgpack_object_print_jsonstr(char *buffer, size_t length, const msgpack_object o)
{
    char *aux_buffer = buffer;
    size_t aux_buffer_size = length;
    size_t ret;

    switch (o.type) {
    case MSGPACK_OBJECT_NIL:
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "null");
        break;

    case MSGPACK_OBJECT_BOOLEAN:
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, (o.via.boolean ? "true" : "false"));
        break;

    case MSGPACK_OBJECT_POSITIVE_INTEGER:
#if defined(PRIu64)
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%" PRIu64, o.via.u64);
#else
        if (o.via.u64 > ULONG_MAX) {
            PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%lu", ULONG_MAX);
        } else {
            PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%lu", (unsigned long)o.via.u64);
        }
#endif
        break;

    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
#if defined(PRIi64)
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%" PRIi64, o.via.i64);
#else
        if (o.via.i64 > LONG_MAX) {
            PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%ld", LONG_MAX);
        } else if (o.via.i64 < LONG_MIN) {
            PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%ld", LONG_MIN);
        } else {
            PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%ld", (signed long)o.via.i64);
        }
#endif
        break;

    case MSGPACK_OBJECT_FLOAT32:
    case MSGPACK_OBJECT_FLOAT64:
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "%f", o.via.f64);
        break;

    case MSGPACK_OBJECT_STR:
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "\"%.*s\"", (int)o.via.str.size, o.via.str.ptr);
        break;

    case MSGPACK_OBJECT_BIN:
        if (bytes_contain_zero(&o.via.bin)) {
            DEBUG("the value contains zero\n");
            return -1;
        }
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "\"%.*s\"", (int)o.via.bin.size, o.via.bin.ptr);
        break;

    case MSGPACK_OBJECT_EXT:
        DEBUG("not support type: MSGPACK_OBJECT_EXT.\n");
        return -1;

    case MSGPACK_OBJECT_ARRAY:
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "[");
        if (o.via.array.size != 0) {
            msgpack_object *p = o.via.array.ptr;
            msgpack_object *const pend = o.via.array.ptr + o.via.array.size;
            PRINT_JSONSTR_CALL(ret, msgpack_object_print_jsonstr, aux_buffer, aux_buffer_size, *p);
            ++p;
            for (; p < pend; ++p) {
                PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, ",");
                PRINT_JSONSTR_CALL(ret, msgpack_object_print_jsonstr, aux_buffer, aux_buffer_size, *p);
            }
        }
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "]");
        break;

    case MSGPACK_OBJECT_MAP:
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "{");
        if (o.via.map.size != 0) {
            msgpack_object_kv *p = o.via.map.ptr;
            msgpack_object_kv *const pend = o.via.map.ptr + o.via.map.size;

            for (; p < pend; ++p) {
                if (p->key.type != MSGPACK_OBJECT_STR) {
                    DEBUG("the key of in a map must be string.\n");
                    return -1;
                }
                if (p != o.via.map.ptr) {
                    PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, ",");
                }
                PRINT_JSONSTR_CALL(ret, msgpack_object_print_jsonstr, aux_buffer, aux_buffer_size, p->key);
                PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, ":");
                PRINT_JSONSTR_CALL(ret, msgpack_object_print_jsonstr, aux_buffer, aux_buffer_size, p->val);
            }
        }
        PRINT_JSONSTR_CALL(ret, snprintf, aux_buffer, aux_buffer_size, "}");
        break;

    default:
        DEBUG("unknown type.\n");
        return -1;
    }

    return length - aux_buffer_size;
}
#undef PRINT_JSONSTR_CALL

static void test(const char *name, const char *input, const char *expect)
{
    msgpack_sbuffer sbuf;
    {
        // pack
        msgpack_packer pk;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
        if (msgpack_pack_jsonstr(&pk, input) < 0) {
            msgpack_sbuffer_destroy(&sbuf);
            printf("%s: invalid json string.\n", name);
            return;
        }
    }

    {
        // unpack
#define MAX_JSONLEN 1024
        msgpack_zone z;
        msgpack_object obj;
        size_t jsonstrlen = MAX_JSONLEN - 1;
        char jsonparsed[MAX_JSONLEN];

        msgpack_zone_init(&z, jsonstrlen);
        msgpack_unpack(sbuf.data, sbuf.size, NULL, &z, &obj);
        jsonstrlen = msgpack_object_print_jsonstr(jsonparsed, jsonstrlen, obj);
        jsonparsed[jsonstrlen] = '\0';

        //compare input and output
        if (expect == NULL) {
            expect = input;
        }
        if (strcmp(expect, jsonparsed) == 0) {
            printf("%s: ok\n", name);
        } else {
            printf("%s: failed\n", name);
        }
        msgpack_zone_destroy(&z);
    }
    msgpack_sbuffer_destroy(&sbuf);
}

int main()
{
    test("null", "null", NULL);
    test("boolean", "false", NULL);
    test("single string", "\"frsyuki\"", NULL);
    test("single number", "\"100\"", NULL);
    test("space", "[{\"valuespace\":\"\",\"\":\"keyspace\"},\"\",[\"\"]]", NULL);
    test("quote", "\"My name is Tom (\\\"Bee\\\") Kobe\"", NULL);
    test("escape", "\"\\\\b\\f\\n\\r\\t\"", NULL);
    test("escape2", "\"\b\f\n\r\t\"", "\"\\b\\f\\n\\r\\t\"");
    test("map", "{\"name\":\"Tom (\\\"Bee\\\") Kobe\",\"type\":\"image\",\"data\":{\"width\":360,\"height\":460,\"title\":\"View me\",\"ips\":[116,943,256,711]}}", NULL);
    test("array", "[\"Sunday\",\"Monday\",\"Tuesday\",\"Wednesday\",\"Thursday\",\"Friday\",\"Saturday\"]", NULL);
    test("number array", "[[101,121,-33],[119,911,171],[0,2,-3]]", NULL);
    test("mix array", "[{\"name\":\"Tom\",\"city\":\"London\",\"country\":\"UK\",\"longitude\":23},{\"name\":\"Jack\",\"city\":\"Birmingham\",\"country\":\"UK\",\"longitude\":-22}]", NULL);
    test("unicode", "\"\\u5C71\\u5DDD\\u7570\\u57DF\\u98A8\\u6708\\u540C\\u5929\"", "\"山川異域風月同天\"");
    test("utf8", "\"山川異域風月同天\"", NULL);
    test("double", "12.34", "12.340000");

    return 0;
}
