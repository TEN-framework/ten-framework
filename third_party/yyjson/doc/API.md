API
===

This document contains all the API usage and examples for the yyjson library.


# API Design

## API prefix

All public functions and structs are prefixed with `yyjson_`, and all constants are prefixed with `YYJSON_`.

## API for immutable/mutable data

The library have 2 types of data structures: immutable and mutable:

|          | Immutable  | Mutable        |
|----------|------------|----------------|
| Document | yyjson_doc | yyjson_mut_doc |
| Value    | yyjson_val | yyjson_mut_val |

When reading a JSON, yyjson returns immutable documents and values.<br/>
When building a JSON, yyjson creates mutable documents and values.<br/>
The document holds the memory for all its JSON values and strings.<br/>

For most immutable APIs, you can just add a `mut` after `yyjson_` to get the mutable version, for example:
```c
char *yyjson_write(yyjson_doc *doc, ...);
char *yyjson_mut_write(yyjson_mut_doc *doc, ...);

bool yyjson_is_str(yyjson_val *val);
bool yyjson_mut_is_str(yyjson_mut_val *val);
```

The library also provides some functions to convert values between immutable and mutable:<br/>

```c
// doc -> mut_doc
yyjson_mut_doc *yyjson_doc_mut_copy(yyjson_doc *doc, ...);
// val -> mut_val
yyjson_mut_val *yyjson_val_mut_copy(yyjson_val *val, ...);

// mut_doc -> doc
yyjson_doc *yyjson_mut_doc_imut_copy(yyjson_mut_doc *doc, ...);
// mut_val -> val
yyjson_doc *yyjson_mut_val_imut_copy(yyjson_mut_val *val, ...);
```

## API for string
The library supports strings with or without null-terminator ('\0').<br/>
When you need to use a string without a null-terminator or when you explicitly know the length of the string, you can use the function that ends with `n`, for example:
```c
// null-terminator is required
bool yyjson_equals_str(yyjson_val *val, const char *str);
// null-terminator is optional
bool yyjson_equals_strn(yyjson_val *val, const char *str, size_t len);
```

When creating JSON, yyjson treats strings as constants for better performance. However, if your string will be modified, you should use a function with a `cpy` to copy the string to the document, for example:
```c
// reference only, null-terminated is required
yyjson_mut_val *yyjson_mut_str(yyjson_mut_doc *doc, const char *str);
// reference only, null-terminator is optional
yyjson_mut_val *yyjson_mut_strn(yyjson_mut_doc *doc, const char *str, size_t len);

// copied, null-terminated is required
yyjson_mut_val *yyjson_mut_strcpy(yyjson_mut_doc *doc, const char *str);
// copied, null-terminator is optional
yyjson_mut_val *yyjson_mut_strncpy(yyjson_mut_doc *doc, const char *str, size_t len);
```



---------------
# Reading JSON
The library provides 4 functions for reading JSON.<br/>
Each function accepts an input of UTF-8 data or a file,<br/>
returns a document if it successful or `NULL` if it fails.

## Read JSON from string
The `dat` should be a UTF-8 string, null-terminator is not required.<br/>
The `len` is the byte length of `dat`.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see `reader flag` for details.<br/>
If input is invalid, `NULL` is returned.

```c
yyjson_doc *yyjson_read(const char *dat, 
                        size_t len, 
                        yyjson_read_flag flg);
```
Sample code:

```c
const char *str = "[1,2,3,4]";
yyjson_doc *doc = yyjson_read(str, strlen(str), 0);
if (doc) {...}
yyjson_doc_free(doc);
```

## Read JSON from file

The `path` is JSON file path.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see `reader flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
If input is invalid, `NULL` is returned.

```c
yyjson_doc *yyjson_read_file(const char *path,
                             yyjson_read_flag flg,
                             const yyjson_alc *alc,
                             yyjson_read_err *err);
```

Sample code:

```c
yyjson_doc *doc = yyjson_read_file("/tmp/test.json", 0, NULL, NULL);
if (doc) {...}
yyjson_doc_free(doc);
```

## Read JSON from file pointer

The `fp` is file pointer. The data will be read from the current position of the FILE to the end.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see `reader flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
If input is invalid, `NULL` is returned.

```c
yyjson_doc *yyjson_read_fp(FILE *fp,
                           yyjson_read_flag flg,
                           const yyjson_alc *alc,
                           yyjson_read_err *err);
```

Sample code:

```c
FILE *fp = fdopen(fd, "rb"); // POSIX file descriptor (fd)
yyjson_doc *doc = yyjson_read_fp(fp, 0, NULL, NULL);
if (fp) fclose(fp);
if (doc) {...}
yyjson_doc_free(doc);
```

## Read JSON with options
The `dat` should be a UTF-8 string, you can pass a const string if you don't use `YYJSON_READ_INSITU` flag.<br/>
The `len` is the `dat`'s length in bytes.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see `reader flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>

```c
yyjson_doc *yyjson_read_opts(char *dat, 
                             size_t len, 
                             yyjson_read_flag flg,
                             const yyjson_alc *alc, 
                             yyjson_read_err *err);
```

Sample code:

```c
const char *dat = your_file.bytes;
size_t len = your_file.size;

yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_INF_AND_NAN;
yyjson_doc *doc = yyjson_read_opts((char *)dat, len, flg, NULL, NULL);

if (doc) {...}

yyjson_doc_free(doc);
```

## Reader error handling

When reading JSON fails and you need error information, you can pass a `yyjson_read_err` pointer to the `yyjson_read_xxx()` functions to receive the error details.

Sample code:
```c
char *dat = ...;
size_t dat_len = ...;
yyjson_read_err err;
yyjson_doc *doc = yyjson_read_opts(dat, dat_len, 0, NULL, &err);

if (!doc) {
    printf("read error: %s, code: %u at byte position: %lu\n", 
            err.msg, err.code, err.pos);
    // printed:
    // read error: trailing comma is not allowed, code: 7, at byte position: 40
}

yyjson_doc_free(doc);
```

The pos in the error information indicates the byte position where the error occurred. If you need the line and column number of the error, you can use the `yyjson_locate_pos()` function. Note that the `line` and `column` start from 1, while `character` starts from 0. All values are calculated based on Unicode characters to ensure compatibility with various text editors.

Sample code:
```c
char *dat = ...;
size_t dat_len = ...;
yyjson_read_err err = ...;

size_t line, col, chr;
if (yyjson_locate_pos(dat, dat_len, err.pos, &line, &col, &chr)) {
    printf("error at line: %lu, column: %lu, character index: %lu\n",
           line, col, chr);
    // printed:
    // error at line: 3, column: 5, character index: 32
}
```

## Reader flag
The library provides a set of flags for JSON reader.<br/>
You can use a single flag, or combine multiple flags with bitwise `|` operator.

● **YYJSON_READ_NOFLAG = 0**<br/>

This is the default flag for JSON reader (RFC-8259 or ECMA-404 compliant):

- Read positive integer as `uint64_t`.
- Read negative integer as `int64_t`.
- Read floating-point number as `double` with correct rounding.
- Read integer which cannot fit in `uint64_t` or `int64_t` as `double`.
- Report error if double number is infinity.
- Report error if string contains invalid UTF-8 character or BOM.
- Report error on trailing commas, comments, `Inf` and `NaN` literals.

● **YYJSON_READ_INSITU**<br/>
Read the input data in-situ.<br/>
This option allows the reader to modify and use the input data to store string values, which can slightly improve reading speed. However, the caller must ensure that the input data is held until the document is freed. The input data must be padded with at least `YYJSON_PADDING_SIZE` bytes. For example: `[1,2]` should be `[1,2]\0\0\0\0`, input length should be 5.

Sample code:

```c
size_t dat_len = ...;
char *buf = malloc(dat_len + YYJSON_PADDING_SIZE); // create a buffer larger than (len + 4)
read_from_socket(buf, ...);
memset(buf + file_size, 0, YYJSON_PADDING_SIZE); // set 4-byte padding after data

yyjson_doc *doc = yyjson_read_opts(buf, dat_len, YYJSON_READ_INSITU, NULL, NULL);
if (doc) {...}
yyjson_doc_free(doc);
free(buf); // the input dat should free after document.
```

● **YYJSON_READ_STOP_WHEN_DONE**<br/>
Stop parsing when reaching the end of a JSON document instead of issues an error if there's additional content after it.<br/> 
This option is useful for parsing small pieces of JSON within larger data, such as [NDJSON](https://en.wikipedia.org/wiki/JSON_streaming).<br/>

Sample code:

```c
// Single file with multiple JSON, such as:
// [1,2,3] [4,5,6] {"a":"b"}

size_t file_size = ...;
char *dat = malloc(file_size + 4);
your_read_file(dat, file);
memset(dat + file_size, 0, 4); // add padding
    
char *hdr = dat;
char *end = dat + file_size;
yyjson_read_flag flg = YYJSON_READ_INSITU | YYJSON_READ_STOP_WHEN_DONE;

while (true) {
    yyjson_doc *doc = yyjson_read_opts(hdr, end - hdr, flg, NULL, NULL);
    if (!doc) break;
    your_doc_process(doc);
    hdr += yyjson_doc_get_read_size(doc); // move to next position
    yyjson_doc_free(doc);
}
free(dat);
```

● **YYJSON_READ_ALLOW_TRAILING_COMMAS**<br/>
Allow a single trailing comma at the end of an object or array (non-standard), for example:

```
{
    "a": 1,
    "b": 2,
}

[
    "a",
    "b",
]
```

● **YYJSON_READ_ALLOW_COMMENTS**<br/>
Allow C-style single line and multiple line comments (non-standard), for example:

```
{
    "name": "Harry", // single line comment
    "id": /* multiple line comment */ 123
}
```

● **YYJSON_READ_ALLOW_INF_AND_NAN**<br/>
Allow nan/inf number or case-insensitive literal (non-standard), for example:

```
{
    "large": 123e999,
    "nan1": NaN,
    "nan2": nan,
    "inf1:" Inf,
    "inf2": -Infinity
}
```

● **YYJSON_READ_NUMBER_AS_RAW**<br/>
Read all numbers as raw strings without parsing.
This flag is useful if you want to handle number parsing yourself.
You can use the following functions to extract raw strings:
```c
bool yyjson_is_raw(yyjson_val *val);
const char *yyjson_get_raw(yyjson_val *val);
size_t yyjson_get_len(yyjson_val *val)
```

● **YYJSON_READ_BIGNUM_AS_RAW**<br/>
Read big numbers as raw strings.
This flag is useful if you want to parse these big numbers yourself.
These big numbers include integers that cannot be represented by `int64_t` and `uint64_t`, and floating-point numbers that cannot be represented by finite `double`.

Note that this flag will be overridden by `YYJSON_READ_NUMBER_AS_RAW` flag.

● **YYJSON_READ_ALLOW_INVALID_UNICODE**<br/>
Allow reading invalid unicode when parsing string values (non-standard),
for example:
```
"\x80xyz"
"\xF0\x81\x81\x81"
```
This flag permits invalid characters to appear in the string values, but it still reports errors for invalid escape sequences. It does not impact the performance of correctly encoded strings.

***Warning***: when using this option, be aware that strings within JSON values may contain incorrect encoding, so you need to handle these strings carefully to avoid security risks.


---------------
# Writing JSON
The library provides 4 sets of functions for writing JSON.<br/>
Each function accepts an input of JSON document or root value, and returns a UTF-8 string or file.

## Write JSON to string
The `doc/val` is JSON document or root value, if you pass NULL, you will get NULL result.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see `writer flag` for details.<br/>
The `len` is a pointer to receive output length (not including the
    null-terminator), pass NULL if you don't need it.<br/>
This function returns a new JSON string, or NULL if error occurs.<br/>
The string is encoded as UTF-8 with a null-terminator. <br/>
You should use free() or alc->free() to release it when it's no longer needed.

```c
// doc -> str
char *yyjson_write(const yyjson_doc *doc, yyjson_write_flag flg, size_t *len);
// mut_doc -> str
char *yyjson_mut_write(const yyjson_mut_doc *doc, yyjson_write_flag flg, size_t *len);
// val -> str
char *yyjson_val_write(const yyjson_val *val, yyjson_write_flag flg, size_t *len);
// mut_val -> str
char *yyjson_mut_val_write(const yyjson_mut_val *val, yyjson_write_flag flg, size_t *len);
```

Sample code 1:

```c
yyjson_doc *doc = yyjson_read("[1,2,3]", 7, 0);
char *json = yyjson_write(doc, YYJSON_WRITE_PRETTY, NULL);
printf("%s\n", json);
free(json);
```

Sample code 2:
```c
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
yyjson_mut_val *arr = yyjson_mut_arr(doc);
yyjson_mut_doc_set_root(doc, arr);
yyjson_mut_arr_add_int(doc, arr, 1);
yyjson_mut_arr_add_int(doc, arr, 2);
yyjson_mut_arr_add_int(doc, arr, 3);
    
char *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, NULL);
printf("%s\n", json);
free(json);
```

## Write JSON to file
The `path` is output JSON file path, If the path is invalid, you will get an error. If the file is not empty, the content will be discarded.<br/>
The `doc/val` is JSON document or root value, if you pass NULL, you will get an error.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see `writer flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
This function returns true on success, or false if error occurs.<br/>

```c
// doc -> file
bool yyjson_write_file(const char *path, const yyjson_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// mut_doc -> file
bool yyjson_mut_write_file(const char *path, const yyjson_mut_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// val -> file
bool yyjson_val_write_file(const char *path, const yyjson_val *val, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// mut_val -> file
bool yyjson_mut_val_write_file(const char *path, const yyjson_mut_val *val, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
```

Sample code:

```c
yyjson_doc *doc = yyjson_read_file("/tmp/test.json", 0, NULL, NULL);
bool suc = yyjson_write_file("tmp/test.json", doc, YYJSON_WRITE_PRETTY, NULL, NULL);
if (suc) printf("OK");
```

## Write JSON to file pointer
The `fp` is output file pointer, The data will be written to the current position of the file.<br/>
The `doc/val` is JSON document or root value, if you pass NULL, you will get an error.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see `writer flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
This function returns true on success, or false if error occurs.<br/>

```c
// doc -> file
bool yyjson_write_fp(FILE *fp, const yyjson_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// mut_doc -> file
bool yyjson_mut_write_fp(FILE *fp, const yyjson_mut_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// val -> file
bool yyjson_val_write_fp(FILE *fp, const yyjson_val *val, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// mut_val -> file
bool yyjson_mut_val_write_fp(FILE *fp, const yyjson_mut_val *val, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
```

Sample code:

```c
FILE *fp = fdopen(fd, "wb"); // POSIX file descriptor (fd)
bool suc = yyjson_write_fp(fp, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
if (fp) fclose(fp);
if (suc) printf("OK");
```

## Write JSON with options
The `doc/val` is JSON document or root value, if you pass NULL, you will get NULL result.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see `writer flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `len` is a pointer to receive output length (not including the
    null-terminator), pass NULL if you don't need it.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>

This function returns a new JSON string, or NULL if error occurs.<br/>
The string is encoded as UTF-8 with a null-terminator. <br/>
You should use free() or alc->free() to release it when it's no longer needed.

```c
char *yyjson_write_opts(const yyjson_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);

char *yyjson_mut_write_opts(const yyjson_mut_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);

char *yyjson_val_write_opts(const yyjson_val *val, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);

char *yyjson_mut_val_write_opts(const yyjson_mut_val *val, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);
```

Sample code:

```c
yyjson_doc *doc = ...;

// init an allocator with stack memory
char buf[64 * 1024];
yyjson_alc alc;
yyjson_alc_pool_init(&alc, buf, sizeof(buf));

// write
size_t len;
yyjson_write_err err;
char *json = yyjson_write_opts(doc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE, &alc, &len, &err);

// get result
if (json) {
    printf("suc: %lu\n%s\n", len, json);
} else {
    printf("err: %u msg:%s\n", err.code, err.msg);
}
alc.free(alc.ctx, json);
```


## Writer flag
The library provides a set of flags for JSON writer.<br/>
You can use a single flag, or combine multiple flags with bitwise `|` operator.

● **YYJSON_WRITE_NOFLAG = 0**<br/>
This is the default flag for JSON writer:

- Writes JSON in minified format.
- Reports an error on encountering `inf` or `nan` number.
- Reports an error on encountering invalid UTF-8 strings.
- Does not escape unicode or slashes.

● **YYJSON_WRITE_PRETTY**<br/>
Writes JSON with a pretty format uing a 4-space indent.

● **YYJSON_WRITE_PRETTY_TWO_SPACES**<br/>
Writes JSON with a pretty format uing a 2-space indent.
This flag will override `YYJSON_WRITE_PRETTY` flag.

● **YYJSON_WRITE_ESCAPE_UNICODE**<br/>
Escape unicode as `\uXXXX`, making the output ASCII-only, for example:

```json
["Alizée, 😊"]
["Aliz\\u00E9e, \\uD83D\\uDE0A"]
```

● **YYJSON_WRITE_ESCAPE_SLASHES**<br/>
Escapes the forward slash character `/` as `\/`, for example:

```json
["https://github.com"]
["https:\/\/github.com"]
```

● **YYJSON_WRITE_ALLOW_INF_AND_NAN**<br/>
Writes inf/nan numbers as `Infinity` and `NaN` literals instead of reporting errors.<br/>

Note that this output is **NOT** standard JSON and may be rejected by other JSON libraries, for example:

```js
{"not_a_number":NaN,"large_number":Infinity}
```

● **YYJSON_WRITE_INF_AND_NAN_AS_NULL**<br/>
Writes inf/nan numbers as `null` literals instead of reporting errors.<br/>
This flag will override `YYJSON_WRITE_ALLOW_INF_AND_NAN` flag, for example:

```js
{"not_a_number":null,"large_number":null}
```

● **YYJSON_WRITE_ALLOW_INVALID_UNICODE**<br/>
Allows invalid unicode when encoding string values.

Invalid characters within string values will be copied byte by byte. If `YYJSON_WRITE_ESCAPE_UNICODE` flag is also set, invalid characters will be escaped as `\uFFFD` (replacement character).

This flag does not affect the performance of correctly encoded string.

● **YYJSON_WRITE_NEWLINE_AT_END**<br/>
Adds a newline character `\n` at the end of the JSON.
This can be helpful for text editors or NDJSON.


---------------
# Accessing JSON Document

## JSON Document

You can access the content of a document with the following functions:
```c
// Get the root value of this JSON document.
yyjson_val *yyjson_doc_get_root(yyjson_doc *doc);

// Get how many bytes are read when parsing JSON.
// e.g. "[1,2,3]" returns 7.
size_t yyjson_doc_get_read_size(yyjson_doc *doc);

// Get total value count in this JSON document.
// e.g. "[1,2,3]" returns 4 (1 array and 3 numbers).
size_t yyjson_doc_get_val_count(yyjson_doc *doc);
```

A document holds all the memory for its internal values and strings. When you no longer need it, you should release the document and free up all the memory:
```c
// Free the document; if NULL is passed in, do nothing.
void yyjson_doc_free(yyjson_doc *doc);
```

## JSON Value

Each JSON Value has a type and subtype, as specified in the table:

| Type             | Subtype              |                         |
| ---------------- | -------------------- | ----------------------- |
| YYJSON_TYPE_NONE |                      | Invalid value           |
| YYJSON_TYPE_RAW  |                      | Raw string              |
| YYJSON_TYPE_NULL |                      | `null` literal          |
| YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_FALSE | `false` literal         |
| YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_TRUE  | `true` literal          |
| YYJSON_TYPE_NUM  | YYJSON_SUBTYPE_UINT  | `uint64_t` nummer       |
| YYJSON_TYPE_NUM  | YYJSON_SUBTYPE_SINT  | `int64_t` number        |
| YYJSON_TYPE_NUM  | YYJSON_SUBTYPE_REAL  | `double` number         |
| YYJSON_TYPE_STR  |                      | String value            |
| YYJSON_TYPE_STR  | YYJSON_SUBTYPE_NOESC | String value, no-escape |
| YYJSON_TYPE_ARR  |                      | Array value             |
| YYJSON_TYPE_OBJ  |                      | Object value            |

- `YYJSON_TYPE_NONE` means invalid value, it does not appear when the JSON is successfully parsed.
- `YYJSON_TYPE_RAW` only appears when the corresponding flag `YYJSON_READ_XXX_AS_RAW` is used.
- `YYJSON_SUBTYPE_NOESC` is used to optimize the writing speed of strings that do not need to be escaped. This subtype is used internally, and the user does not need to handle it.

The following functions can be used to determine the type of a JSON value.

```c
// Returns the type and subtype of a JSON value.
// Returns 0 if the input is NULL.
yyjson_type yyjson_get_type(yyjson_val *val);
yyjson_subtype yyjson_get_subtype(yyjson_val *val);

// Returns value's tag, see `Data Structures` doc for details.
uint8_t yyjson_get_tag(yyjson_val *val);

// returns type description, such as:  
// "null", "string", "array", "object", "true", "false",
// "uint", "sint", "real", "unknown"
const char *yyjson_get_type_desc(yyjson_val *val);

// Returns true if the JSON value is specified type.
// Returns false if the input is NULL or not the specified type.
bool yyjson_is_null(yyjson_val *val);  // null
bool yyjson_is_true(yyjson_val *val);  // true
bool yyjson_is_false(yyjson_val *val); // false
bool yyjson_is_bool(yyjson_val *val);  // true/false
bool yyjson_is_uint(yyjson_val *val);  // uint64_t
bool yyjson_is_sint(yyjson_val *val);  // int64_t
bool yyjson_is_int(yyjson_val *val);   // uint64_t/int64_t
bool yyjson_is_real(yyjson_val *val);  // double
bool yyjson_is_num(yyjson_val *val);   // uint64_t/int64_t/double
bool yyjson_is_str(yyjson_val *val);   // string
bool yyjson_is_arr(yyjson_val *val);   // array
bool yyjson_is_obj(yyjson_val *val);   // object
bool yyjson_is_ctn(yyjson_val *val);   // array/object
bool yyjson_is_raw(yyjson_val *val);   // raw string
```

The following functions can be used to get the contents of the JSON value.

```c
// Returns the raw string, or NULL if `val` is not raw type.
const char *yyjson_get_raw(yyjson_val *val);

// Returns bool value, or false if `val` is not bool type.
bool yyjson_get_bool(yyjson_val *val);

// Returns uint64_t value, or 0 if `val` is not uint type.
uint64_t yyjson_get_uint(yyjson_val *val);

// Returns int64_t value, or 0 if `val` is not sint type.
int64_t yyjson_get_sint(yyjson_val *val);

// Returns int value (may overflow), or 0 if `val` is not uint/sint type.
int yyjson_get_int(yyjson_val *val);

// Returns double value, or 0 if `val` is not real type.
double yyjson_get_real(yyjson_val *val);

// Returns double value (typecast), or 0 if `val` is not uint/sint/real type.
double yyjson_get_num(yyjson_val *val);

// Returns the string value, or NULL if `val` is not string type.
const char *yyjson_get_str(yyjson_val *val);

// Returns the content length (string length in bytes, array size, 
// object size), or 0 if the value does not contains length data.
size_t yyjson_get_len(yyjson_val *val);

// Returns whether the value is equals to a string.
// Returns false if input is NULL or `val` is not string.
bool yyjson_equals_str(yyjson_val *val, const char *str);
bool yyjson_equals_strn(yyjson_val *val, const char *str, size_t len);
```


The following functions can be used to modify the content of a JSON value.<br/>

Warning: For immutable documents, these functions will break the `immutable` convention, you should use this set of APIs with caution (e.g. make sure the document is only accessed in a single thread).

```c
// Set the value to new type and content.
// Returns false if input is NULL or `val` is object or array.
bool yyjson_set_raw(yyjson_val *val, const char *raw, size_t len);
bool yyjson_set_null(yyjson_val *val);
bool yyjson_set_bool(yyjson_val *val, bool num);
bool yyjson_set_uint(yyjson_val *val, uint64_t num);
bool yyjson_set_sint(yyjson_val *val, int64_t num);
bool yyjson_set_int(yyjson_val *val, int num);
bool yyjson_set_real(yyjson_val *val, double num);

// The string is not copied, should be held by caller.
bool yyjson_set_str(yyjson_val *val, const char *str);
bool yyjson_set_strn(yyjson_val *val, const char *str, size_t len);
```


## JSON Array

The following functions can be used to access a JSON array.<br/>

Note that accessing elements by index may take a linear search time. Therefore, if you need to iterate through an array, it is recommended to use the iterator API.

```c
// Returns the number of elements in this array.
// Returns 0 if the input is not an array.
size_t yyjson_arr_size(yyjson_val *arr);

// Returns the element at the specified position (linear search time).
// Returns NULL if the index is out of bounds, or input is not an array.
yyjson_val *yyjson_arr_get(yyjson_val *arr, size_t idx);

// Returns the first element of this array (constant time).
// Returns NULL if array is empty or intput is not an array.
yyjson_val *yyjson_arr_get_first(yyjson_val *arr);

// Returns the last element of this array (linear search time).
// Returns NULL if array is empty or intput is not an array.
yyjson_val *yyjson_arr_get_last(yyjson_val *arr);
```

## JSON Array Iterator
There are two ways to traverse an array:<br/>

Sample code 1 (iterator API):
```c
yyjson_val *arr; // the array to be traversed

yyjson_val *val;
yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
while ((val = yyjson_arr_iter_next(&iter))) {
    your_func(val);
}
```

Sample code 2 (foreach macro):
```c
yyjson_val *arr; // the array to be traversed

size_t idx, max;
yyjson_val *val;
yyjson_arr_foreach(arr, idx, max, val) {
    your_func(idx, val);
}
```
<br/>

There's also mutable version API to traverse an mutable array:<br/>

Sample code 1 (mutable iterator API):
```c
yyjson_mut_val *arr; // the array to be traversed

yyjson_mut_val *val;
yyjson_mut_arr_iter iter = yyjson_mut_arr_iter_with(arr);
while ((val = yyjson_mut_arr_iter_next(&iter))) {
    if (your_val_is_unused(val)) {
        // you can remove current value inside iteration
        yyjson_mut_arr_iter_remove(&iter); 
    }
}
```

Sample code 2 (mutable foreach macro):
```c
yyjson_mut_val *arr; // the array to be traversed

size_t idx, max;
yyjson_mut_val *val;
yyjson_mut_arr_foreach(arr, idx, max, val) {
    your_func(idx, val);
}
```


## JSON Object
The following functions can be used to access a JSON object.<br/>

Note that accessing elements by key may take a linear search time. Therefore, if you need to iterate through an object, it is recommended to use the iterator API.


```c
// Returns the number of key-value pairs in this object.
// Returns 0 if input is not an object.
size_t yyjson_obj_size(yyjson_val *obj);

// Returns the value to which the specified key is mapped.
// Returns NULL if this object contains no mapping for the key.
yyjson_val *yyjson_obj_get(yyjson_val *obj, const char *key);
yyjson_val *yyjson_obj_getn(yyjson_val *obj, const char *key, size_t key_len);

// If the order of object's key is known at compile-time,
// you can use this method to avoid searching the entire object.
// e.g. { "x":1, "y":2, "z":3 }
yyjson_val *obj = ...;
yyjson_obj_iter iter = yyjson_obj_iter_with(obj);

yyjson_val *x = yyjson_obj_iter_get(&iter, "x");
yyjson_val *z = yyjson_obj_iter_get(&iter, "z");
```

## JSON Object Iterator
There are two ways to traverse an object:<br/>

Sample code 1 (iterator API):
```c
yyjson_val *obj; // the object to be traversed

yyjson_val *key, *val;
yyjson_obj_iter iter = yyjson_obj_iter_with(obj);
while ((key = yyjson_obj_iter_next(&iter))) {
    val = yyjson_obj_iter_get_val(key);
    your_func(key, val);
}
```

Sample code 2 (foreach macro):
```c
yyjson_val *obj; // this is your object

size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    your_func(key, val);
}
```
<br/>

There's also mutable version API to traverse an mutable object:<br/>

Sample code 1 (mutable iterator API):
```c
yyjson_mut_val *obj; // the object to be traversed

yyjson_mut_val *key, *val;
yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(obj);
while ((key = yyjson_mut_obj_iter_next(&iter))) {
    val = yyjson_mut_obj_iter_get_val(key);
    if (your_key_is_unused(key)) {
        // you can remove current kv pair inside iteration
        yyjson_mut_obj_iter_remove(&iter);
    }
}
```

Sample code 2 (mutable foreach macro):
```c
yyjson_mut_val *obj; // the object to be traversed

size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    your_func(key, val);
}
```


---------------
# Creating JSON Document
The `yyjson_mut_doc` and related APIs are used to build JSON documents. <br/>

Please note that `yyjson_mut_doc` uses a **memory pool** to hold all strings and values. The pool can only be created, grown, or freed in its entirety. Therefore, `yyjson_mut_doc` is more suitable for write-once than mutation of an existing document.<br/>

JSON objects and arrays are composed of linked lists, so each `yyjson_mut_val` can only be added to one object or array.

Sample code:

```c
// Build this JSON:
//     {
//        "page": 123,
//        "names": [ "Harry", "Ron", "Hermione" ]
//     }

// Create a mutable document.
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

// Create an object, the value's memory is held by doc.
yyjson_mut_val *root = yyjson_mut_obj(doc);

// Create key and value, add to the root object.
yyjson_mut_val *key = yyjson_mut_str(doc, "page");
yyjson_mut_val *num = yyjson_mut_int(doc, 123);
yyjson_mut_obj_add(root, key, num);

// Create 3 string value, add to the array object.
yyjson_mut_val *names = yyjson_mut_arr(doc);
yyjson_mut_val *name1 = yyjson_mut_str(doc, "Harry");
yyjson_mut_val *name2 = yyjson_mut_str(doc, "Ron");
yyjson_mut_val *name3 = yyjson_mut_str(doc, "Hermione");
yyjson_mut_arr_append(names, name1);
yyjson_mut_arr_append(names, name2);
yyjson_mut_arr_append(names, name3);

// ❌ Wrong! the value is already added to another container.
yyjson_mut_obj_add(root, key, name1);

// Set the document's root value.
yyjson_mut_doc_set_root(doc, root);

// Write to JSON string
const char *json = yyjson_mut_write(doc, 0, NULL);

// Free the memory of doc and all values which is created from this doc.
yyjson_mut_doc_free(doc);
```


## Mutable Document

The following functions are used to create, modify, copy, and destroy a JSON document.<br/>

```c
// Creates and returns a new mutable JSON document.
// Returns NULL on error (e.g. memory allocation failure).
// If `alc` is NULL, the default allocator will be used.
yyjson_mut_doc *yyjson_mut_doc_new(yyjson_alc *alc);

// Delete the JSON document, free the memory of this doc
// and all values created from this doc
void yyjson_mut_doc_free(yyjson_mut_doc *doc);

// Set the internal memory pool size (string length and value count).
// It can be used to reserve memory for the next string and value creation.
bool yyjson_mut_doc_set_str_pool_size(yyjson_mut_doc *doc, size_t len);
bool yyjson_mut_doc_set_val_pool_size(yyjson_mut_doc *doc, size_t count);

// Get or set the root value of this JSON document.
yyjson_mut_val *yyjson_mut_doc_get_root(yyjson_mut_doc *doc);
void yyjson_mut_doc_set_root(yyjson_mut_doc *doc, yyjson_mut_val *root);

// Copies and returns a new mutable document/value from input.
// Returns NULL on error (e.g. memory allocation failure).

// doc -> mut_doc
yyjson_mut_doc *yyjson_doc_mut_copy(yyjson_doc *doc, const yyjson_alc *alc);
// val -> mut_val
yyjson_mut_val *yyjson_val_mut_copy(yyjson_mut_doc *doc,  yyjson_val *val);
// mut_doc -> mut_doc
yyjson_mut_doc *yyjson_mut_doc_mut_copy(yyjson_mut_doc *doc, const yyjson_alc *alc);
// mut_val -> mut_val
yyjson_mut_val *yyjson_mut_val_mut_copy(yyjson_mut_doc *doc, yyjson_mut_val *val);
// mut_doc -> doc
yyjson_doc *yyjson_mut_doc_imut_copy(yyjson_mut_doc *doc, yyjson_alc *alc);
// mut_val -> doc
yyjson_doc *yyjson_mut_val_imut_copy(yyjson_mut_val *val, yyjson_alc *alc);
```

## JSON Value Creation
The following functions are used to create mutable JSON value, 
the value's memory is held by the document.<br/>

```c
// Creates and returns a new value, returns NULL on error.
yyjson_mut_val *yyjson_mut_null(yyjson_mut_doc *doc);
yyjson_mut_val *yyjson_mut_true(yyjson_mut_doc *doc);
yyjson_mut_val *yyjson_mut_false(yyjson_mut_doc *doc);
yyjson_mut_val *yyjson_mut_bool(yyjson_mut_doc *doc, bool val);
yyjson_mut_val *yyjson_mut_uint(yyjson_mut_doc *doc, uint64_t num);
yyjson_mut_val *yyjson_mut_sint(yyjson_mut_doc *doc, int64_t num);
yyjson_mut_val *yyjson_mut_int(yyjson_mut_doc *doc, int64_t num);
yyjson_mut_val *yyjson_mut_real(yyjson_mut_doc *doc, double num);

// Creates a string value, the input string is NOT copied.
yyjson_mut_val *yyjson_mut_str(yyjson_mut_doc *doc, const char *str);
yyjson_mut_val *yyjson_mut_strn(yyjson_mut_doc *doc, const char *str, size_t len);

// Creates a string value, the input string is copied and held by the document.
yyjson_mut_val *yyjson_mut_strcpy(yyjson_mut_doc *doc, const char *str);
yyjson_mut_val *yyjson_mut_strncpy(yyjson_mut_doc *doc, const char *str, size_t len);
```


## JSON Array Creation
The following functions are used to create mutable JSON array.<br/>

```c
// Creates and returns an empty mutable array, returns NULL on error.
yyjson_mut_val *yyjson_mut_arr(yyjson_mut_doc *doc);

// Creates and returns a mutable array with c array.
yyjson_mut_val *yyjson_mut_arr_with_bool(yyjson_mut_doc *doc, bool *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint(yyjson_mut_doc *doc, int64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint(yyjson_mut_doc *doc, uint64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_real(yyjson_mut_doc *doc, double *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint8(yyjson_mut_doc *doc, int8_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint16(yyjson_mut_doc *doc, int16_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint32(yyjson_mut_doc *doc, int32_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint64(yyjson_mut_doc *doc, int64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint8(yyjson_mut_doc *doc, uint8_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint16(yyjson_mut_doc *doc, uint16_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint32(yyjson_mut_doc *doc, uint32_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint64(yyjson_mut_doc *doc, uint64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_float(yyjson_mut_doc *doc, float *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_double(yyjson_mut_doc *doc, double *vals, size_t count);
// sample code:
int vals[3] = {-1, 0, 1};
yyjson_mut_val *arr = yyjson_mut_arr_with_sint32(doc, vals, 3);

// Creates and returns a mutable array with strings,
// the strings should be encoded as UTF-8.
yyjson_mut_val *yyjson_mut_arr_with_str(yyjson_mut_doc *doc, const char **vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strn(yyjson_mut_doc *doc, const char **vals, const size_t *lens, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strcpy(yyjson_mut_doc *doc, const char **vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strncpy(yyjson_mut_doc *doc, const char **vals, const size_t *lens, size_t count);
// sample code:
const char strs[3] = {"Jan", "Feb", "Mar"};
yyjson_mut_val *arr = yyjson_mut_arr_with_str(doc, strs, 3);
```

## JSON Array Modification

The following functions are used to modify the contents of a JSON array.<br/>

```c
// Inserts a value into an array at a given index.
// Returns false on error (e.g. out of bounds).
// Note that this function takes a linear search time.
bool yyjson_mut_arr_insert(yyjson_mut_val *arr, yyjson_mut_val *val, size_t idx);

// Inserts a val at the end of the array, returns false on error.
bool yyjson_mut_arr_append(yyjson_mut_val *arr, yyjson_mut_val *val);

// Inserts a val at the head of the array, returns false on error.
bool yyjson_mut_arr_prepend(yyjson_mut_val *arr, yyjson_mut_val *val);

// Replaces a value at index and returns old value, returns NULL on error.
// Note that this function takes a linear search time.
yyjson_mut_val *yyjson_mut_arr_replace(yyjson_mut_val *arr, size_t idx, yyjson_mut_val *val);

// Removes and returns a value at index, returns NULL on error.
// Note that this function takes a linear search time.
yyjson_mut_val *yyjson_mut_arr_remove(yyjson_mut_val *arr, size_t idx);

// Removes and returns the first value in this array, returns NULL on error.
yyjson_mut_val *yyjson_mut_arr_remove_first(yyjson_mut_val *arr);

// Removes and returns the last value in this array, returns NULL on error.
yyjson_mut_val *yyjson_mut_arr_remove_last(yyjson_mut_val *arr);

// Removes all values within a specified range in the array.
// Note that this function takes a linear search time.
bool yyjson_mut_arr_remove_range(yyjson_mut_val *arr, size_t idx, size_t len);

// Removes all values in this array.
bool yyjson_mut_arr_clear(yyjson_mut_val *arr);

// Convenience API:
// Adds a value at the end of this array, returns false on error.
bool yyjson_mut_arr_add_val(yyjson_mut_val *arr, yyjson_mut_val *val);
bool yyjson_mut_arr_add_null(yyjson_mut_doc *doc, yyjson_mut_val *arr);
bool yyjson_mut_arr_add_true(yyjson_mut_doc *doc, yyjson_mut_val *arr);
bool yyjson_mut_arr_add_false(yyjson_mut_doc *doc, yyjson_mut_val *arr);
bool yyjson_mut_arr_add_bool(yyjson_mut_doc *doc, yyjson_mut_val *arr, bool val);
bool yyjson_mut_arr_add_uint(yyjson_mut_doc *doc, yyjson_mut_val *arr, uint64_t num);
bool yyjson_mut_arr_add_sint(yyjson_mut_doc *doc, yyjson_mut_val *arr, int64_t num);
bool yyjson_mut_arr_add_int(yyjson_mut_doc *doc, yyjson_mut_val *arr, int64_t num);
bool yyjson_mut_arr_add_real(yyjson_mut_doc *doc, yyjson_mut_val *arr, double num);
bool yyjson_mut_arr_add_str(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str);
bool yyjson_mut_arr_add_strn(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str, size_t len);
bool yyjson_mut_arr_add_strcpy(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str);
bool yyjson_mut_arr_add_strncpy(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str, size_t len);

// Convenience API:
// Creates and adds a new array at the end of the array.
// Returns the new array, or NULL on error.
yyjson_mut_val *yyjson_mut_arr_add_arr(yyjson_mut_doc *doc, yyjson_mut_val *arr);

// Convenience API:
// Creates and adds a new object at the end of the array.
// Returns the new object, or NULL on error.
yyjson_mut_val *yyjson_mut_arr_add_obj(yyjson_mut_doc *doc, yyjson_mut_val *arr);
```

## JSON Object Creation
The following functions are used to create mutable JSON object.<br/>

```c
// Creates and returns a mutable object, returns NULL on error.
yyjson_mut_val *yyjson_mut_obj(yyjson_mut_doc *doc);

// Creates and returns a mutable object with keys and values,
// returns NULL on error. The keys and values are NOT copied.
// The strings should be encoded as UTF-8 with null-terminator.
yyjson_mut_val *yyjson_mut_obj_with_str(yyjson_mut_doc *doc,
                                        const char **keys,
                                        const char **vals,
                                        size_t count);
// sample code:
const char keys[] = {"name", "type", "id"};
const char *vals[] = {"Harry", "student", "123456"};
yyjson_mut_obj_with_str(doc, keys, vals, 3);

// Creates and returns a mutable object with key-value pairs,
// returns NULL on error. The keys and values are NOT copied.
// The strings should be encoded as UTF-8 with null-terminator.
yyjson_mut_val *yyjson_mut_obj_with_kv(yyjson_mut_doc *doc,
                                       const char **kv_pairs,
                                       size_t pair_count);
// sample code:
const char *pairs[] = {"name", "Harry", "type", "student", "id", "123456"};
yyjson_mut_obj_with_kv(doc, pairs, 3);
```

## JSON Object Modification
The following functions are used to modify the contents of a JSON object.<br/>

```c
// Adds a key-value pair at the end of the object. 
// The key must be a string value.
// This function allows duplicated key in one object.
bool yyjson_mut_obj_add(yyjson_mut_val *obj, yyjson_mut_val *key,yyjson_mut_val *val);

// Adds a key-value pair to the object.
// The key must be a string value.
// This function may remove all key-value pairs for the given key before add.
// Note that this function takes a linear search time.
bool yyjson_mut_obj_put(yyjson_mut_val *obj, yyjson_mut_val *key, yyjson_mut_val *val);

// Removes key-value pair from the object with a given key.
// Note that this function takes a linear search time.
bool yyjson_mut_obj_remove(yyjson_mut_val *obj, yyjson_mut_val *key);

// Removes all key-value pairs in this object.
bool yyjson_mut_obj_clear(yyjson_mut_val *obj);

// Convenience API:
// Adds a key-value pair at the end of the object. The key is not copied.
// Note that these functions allow duplicated key in one object.
bool yyjson_mut_obj_add_null(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_add_true(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_add_false(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_add_bool(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool val);
bool yyjson_mut_obj_add_uint(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, uint64_t val);
bool yyjson_mut_obj_add_sint(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t val);
bool yyjson_mut_obj_add_int(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t val);
bool yyjson_mut_obj_add_real(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, double val);
bool yyjson_mut_obj_add_str(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val);
bool yyjson_mut_obj_add_strn(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val, size_t len);
bool yyjson_mut_obj_add_strcpy(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val);
bool yyjson_mut_obj_add_strncpy(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val, size_t len);
yyjson_mut_val *yyjson_mut_obj_add_arr(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *_key);
yyjson_mut_val *yyjson_mut_obj_add_obj(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *_key);
                              
// Convenience API:
// Removes all key-value pairs for the given key.
// Note that this function takes a linear search time.
bool yyjson_mut_obj_remove_str(yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_remove_strn(yyjson_mut_val *obj, const char *key, size_t len);

// Convenience API:
// Replaces all matching keys with the new key.
// Returns true if at least one key was renamed.
// This function takes a linear search time.
yyjson_api_inline bool yyjson_mut_obj_rename_key(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *new_key);
yyjson_api_inline bool yyjson_mut_obj_rename_keyn(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, size_t len, const char *new_key, size_t new_len);
```


---------------
# JSON Pointer and Patch

## JSON Pointer
The library supports querying JSON values using `JSON Pointer` ([RFC 6901](https://tools.ietf.org/html/rfc6901)).

```c
// `JSON pointer` is a null-terminated string.
yyjson_val *yyjson_ptr_get(yyjson_val *val, const char *ptr);
yyjson_val *yyjson_doc_ptr_get(yyjson_doc *doc, const char *ptr);
yyjson_mut_val *yyjson_mut_ptr_get(yyjson_mut_val *val, const char *ptr);
yyjson_mut_val *yyjson_mut_doc_ptr_get(yyjson_mut_doc *doc, const char *ptr);

// `JSON pointer` with string length, allow NUL (Unicode U+0000) characters inside.
yyjson_val *yyjson_ptr_getn(yyjson_val *val, const char *ptr, size_t len);
yyjson_val *yyjson_doc_ptr_getn(yyjson_doc *doc, const char *ptr, size_t len);
yyjson_mut_val *yyjson_mut_ptr_getn(yyjson_mut_val *val, const char *ptr, size_t len);
yyjson_mut_val *yyjson_mut_doc_ptr_getn(yyjson_mut_doc *doc, const char *ptr, size_t len);

// `JSON pointer` with string length, context and error information.
yyjson_val *yyjson_ptr_getx(yyjson_val *val, const char *ptr, size_t len, yyjson_ptr_err *err);
yyjson_val *yyjson_doc_ptr_getx(yyjson_doc *doc, const char *ptr, size_t len, yyjson_ptr_err *err);
yyjson_mut_val *yyjson_mut_ptr_getx(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);
yyjson_mut_val *yyjson_mut_doc_ptr_getx(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);
```

For example, given the JSON document:
```json
{
    "size" : 3,
    "users" : [
        {"id": 1, "name": "Harry"},
        {"id": 2, "name": "Ron"},
        {"id": 3, "name": "Hermione"}
    ]
}
```
The following JSON strings evaluate to the accompanying values:

|Pointer|Matched Value|
|:--|:--|
| `""` | `the whole document` |
| `"/size"`| `3` |
| `"/users/0"` | `{"id": 1, "name": "Harry"}` |
| `"/users/1/name"` | `"Ron"` |
| `"/no_match"` | NULL |
| `"no_slash"` | NULL |
| `"/"` | NULL (match to empty key: root[""]) |

```c
yyjson_doc *doc = ...;
yyjson_val *val = yyjson_doc_ptr_get(doc, "/users/1/name");
printf("%s\n", yyjson_get_str(val)); // Ron

yyjson_ptr_err err;
yyjson_val *val2 = yyjson_doc_ptr_getx(doc, "/", 1, &err);
if (!val2) printf("err %d: %s\n", err.code, err.msg); // err 3: cannot be resolved
```

The library also supports modifying JSON values using `JSON Pointer`.
```c
// Add or insert a new value.
bool yyjson_mut_ptr_add(yyjson_mut_val *val, const char *ptr, yyjson_mut_val *new_val, yyjson_mut_doc *doc);
bool yyjson_mut_ptr_addn(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_mut_val *new_val, yyjson_mut_doc *doc);
bool yyjson_mut_ptr_addx(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_mut_val *new_val, yyjson_mut_doc *doc, bool create_parent, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);
                                           
bool yyjson_mut_doc_ptr_add(yyjson_mut_doc *doc, const char *ptr, yyjson_mut_val *new_val);
bool yyjson_mut_doc_ptr_addn(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_mut_val *new_val);
bool yyjson_mut_doc_ptr_addx(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_mut_val *new_val, bool create_parent, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);

// Set a new value (add if it doesn't exist, replace if it does).
bool yyjson_mut_ptr_set(yyjson_mut_val *val, const char *ptr, yyjson_mut_val *new_val, yyjson_mut_doc *doc);
bool yyjson_mut_ptr_setn(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_mut_val *new_val, yyjson_mut_doc *doc);
bool yyjson_mut_ptr_setx(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_mut_val *new_val, yyjson_mut_doc *doc, bool create_parent, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);
                                             
bool yyjson_mut_doc_ptr_set(yyjson_mut_doc *doc, const char *ptr, yyjson_mut_val *new_val);
bool yyjson_mut_doc_ptr_setn(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_mut_val *new_val);
bool yyjson_mut_doc_ptr_setx(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_mut_val *new_val, bool create_parent, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);

// Replace an existing value.
yyjson_mut_val *yyjson_mut_ptr_replace(yyjson_mut_val *val, const char *ptr, yyjson_mut_val *new_val);
yyjson_mut_val *yyjson_mut_ptr_replacen(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_mut_val *new_val);
yyjson_mut_val *yyjson_mut_ptr_replacex(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_mut_val *new_val, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);
    
yyjson_mut_val *yyjson_mut_doc_ptr_replace(yyjson_mut_doc *doc, const char *ptr, yyjson_mut_val *new_val);
yyjson_mut_val *yyjson_mut_doc_ptr_replacen(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_mut_val *new_val);
yyjson_mut_val *yyjson_mut_doc_ptr_replacex(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_mut_val *new_val, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);

// Remove an existing value.
yyjson_mut_val *yyjson_mut_ptr_remove(yyjson_mut_val *val, const char *ptr);
yyjson_mut_val *yyjson_mut_ptr_removen(yyjson_mut_val *val, const char *ptr, size_t len);
yyjson_mut_val *yyjson_mut_ptr_removex(yyjson_mut_val *val, const char *ptr, size_t len, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);

yyjson_mut_val *yyjson_mut_doc_ptr_remove(yyjson_mut_doc *doc, const char *ptr);
yyjson_mut_val *yyjson_mut_doc_ptr_removen(yyjson_mut_doc *doc, const char *ptr, size_t len);
yyjson_mut_val *yyjson_mut_doc_ptr_removex(yyjson_mut_doc *doc, const char *ptr, size_t len, yyjson_ptr_ctx *ctx, yyjson_ptr_err *err);
```

For example:
```c
yyjson_mut_doc *doc = ...;
// doc: {"a":0,"b":[1,2,3]}

yyjson_mut_doc_ptr_set(doc, "/a", yyjson_mut_int(doc, 9));
// now: {"a":9,"b":[1,2,3]}

yyjson_mut_doc_ptr_add(doc, "/b/-", yyjson_mut_int(doc, 4));
// now: {"a":9,"b":[1,2,3,4]}

yyjson_mut_doc_ptr_remove(doc, "/b");
// now: {"a":9}
```

All the above functions ending with `x` can be used to get the result context `ctx`, and the error message `err`. For example:
```c
// doc: {"a":0,"b":[null,2,3]}
yyjson_mut_doc *doc = ...;

// get error code and message
yyjson_ptr_err err;
yyjson_mut_doc_ptr_setx(doc, "/b/99", 4, yyjson_mut_int(doc, 99), true, NULL, &err);
if (err.code) printf("err: %s\n", err.msg); // err: cannot resolve

// get target value's context
// perform some operations without re-parsing the JSON Pointer
yyjson_mut_val *val = yyjson_mut_doc_ptr_getx(doc, "/b/0", 4, &ctx, &err);
if (yyjson_mut_is_null(val)) yyjson_ptr_ctx_remove(&ctx);
// now: {"a":0,"b":[2,3]}
```



## JSON Patch
The library supports JSON Patch (RFC 6902).
Specification and example: <https://tools.ietf.org/html/rfc6902>
```c
// Creates and returns a patched JSON value.
// Returns NULL if the patch could not be applied.
yyjson_mut_val *yyjson_patch(yyjson_mut_doc *doc,
                             yyjson_val *orig,
                             yyjson_val *patch,
                             yyjson_patch_err *err);

yyjson_mut_val *yyjson_mut_patch(yyjson_mut_doc *doc,
                                 yyjson_mut_val *orig,
                                 yyjson_mut_val *patch,
                                 yyjson_patch_err *err);
```


## JSON Merge Patch
The library supports JSON Merge Patch (RFC 7386).
Specification and example: <https://tools.ietf.org/html/rfc7386>
```c
// Creates and returns a merge-patched JSON value.
// Returns NULL if the patch could not be applied.
yyjson_mut_val *yyjson_merge_patch(yyjson_mut_doc *doc,
                                   yyjson_val *orig,
                                   yyjson_val *patch);

yyjson_mut_val *yyjson_mut_merge_patch(yyjson_mut_doc *doc,
                                       yyjson_mut_val *orig,
                                       yyjson_mut_val *patch);
```


---------------
# Number Processing

## Number reader
The library has a built-in high-performance number reader,<br/>
it will read numbers according to these rules by default:<br/>

* Positive integers are read as `uint64_t`. If an overflow occurs, it is converted to `double`.
* Negative integers are read as `int64_t`. If an overflow occurs, it is converted to `double`.
* Floating-point numbers are read as `double` with correct rounding.
* If a `double` number overflow (reaches infinity), an error is reported.
* If a number does not conform to the [JSON](https://www.json.org) standard, an error is reported.

There are 3 flags that can be used to adjust the number parsing strategy:

- `YYJSON_READ_ALLOW_INF_AND_NAN`: read nan/inf number or literal as `double` (non-standard).
- `YYJSON_READ_NUMBER_AS_RAW`: read all numbers as raw strings without parsing.
- `YYJSON_READ_BIGBER_AS_RAW`: read big numbers (overflow or infinity) as raw strings without parsing.

See the `Reader flag` section for more details.

## Number writer
The library has a built-in high-performance number writer,<br/>
it will write numbers according to these rules by default:<br/>

* Positive integers are written without a sign.
* Negative integers are written with a negative sign.
* Floating-point numbers are written using the [ECMAScript format](https://www.ecma-international.org/ecma-262/11.0/index.html#sec-numeric-types-number-tostring), with the following modifications:
    * If the number is `Infinity` or `NaN`, an error is reported.
    * The negative sign of `-0.0` is preserved to maintain input information.
    * The positive sign in the exponent part is removed.
* The floating-point number writer will generate the shortest correctly rounded decimal representation.

There are 2 flags that can be used to adjust the number writing strategy:

- `YYJSON_WRITE_ALLOW_INF_AND_NAN` write inf/nan numbers as `Infinity` and `NaN` literals without error (non-standard).
- `YYJSON_WRITE_INF_AND_NAN_AS_NULL` write inf/nan numbers as `null` literal.

See the "Writer flag" section for more details.



# Text Processing

## Character Encoding
This library only supports UTF-8 encoding without BOM, as specified in [RFC 8259](https://datatracker.ietf.org/doc/html/rfc8259#section-8.1):

> JSON text exchanged between systems that are not part of a closed ecosystem MUST be encoded using UTF-8.
> Implementations MUST NOT add a byte order mark (U+FEFF) to the beginning of a networked-transmitted JSON text.

By default, yyjson performs strict UTF-8 encoding validation on input strings. If an invalid character is encountered, an error will be reported.

You can use `YYJSON_READ_ALLOW_INVALID_UNICODE` and `YYJSON_WRITE_ALLOW_INVALID_UNICODE` flags to allow invalid Unicode encoding. However, please note that if you enable these flags, the resulting value from yyjson may contain invalid characters, which can be used by other code and may introduce security risks.

## NUL Character
This library supports the `NUL` character (also known as the `null terminator`, or Unicode `U+0000`, ASCII `\0`) inside strings.

When reading JSON, `\u0000` will be unescaped to `NUL` character. If a string contains the `NUL` character, the length obtained with `strlen()` will be inaccurate, and you should use `yyjson_get_len()` to get the actual length.

When building JSON, the input string is treated as null-terminated by default. If you need to pass in a string that contains the `NUL` character, you should use the API with the `n` suffix and provide the actual length of the string.

For example:
```c
// null-terminated string
yyjson_mut_str(doc, str);
yyjson_obj_get(obj, str);

// any string, with or without null terminator
yyjson_mut_strn(doc, str, len);
yyjson_obj_getn(obj, str, len);

// C++ string
std::string sstr = ...;
yyjson_obj_getn(obj, sstr.data(), sstr.length());
```



# Memory Allocator
The library does not directly call libc's memory allocation functions (malloc/realloc/free). Instead, when memory allocation is required, yyjson's API takes a parameter named `alc` that allows the caller to pass in an allocator. If the `alc` is NULL, yyjson will use the default memory allocator, which is a simple wrapper of libc's functions.

Using a custom memory allocator allows you to have more control over memory allocation, here are a few examples:


## Single allocator for multiple JSON
If you need to parse multiple small JSON one by one, you can use a single allocator to avoid multiple memory allocations.

Sample code:
```c
// max data size for single JSON
size_t max_json_size = 64 * 1024;
// calculate the max memory usage for a single JSON
size_t buf_size = yyjson_read_max_memory_usage(max_json_size, 0);
// create a buffer for allocator
void *buf = malloc(buf_size);
// setup the allocator with buffer
yyjson_alc alc;
yyjson_alc_pool_init(&alc, buf, buf_size);

// read multiple JSON using one allocator
for(int i = 0, i < your_json_file_count; i++) {
    const char *your_json_file_path = ...;
    yyjson_doc *doc = yyjson_read_file(your_json_file_path, 0, &alc, NULL);
    ...
    yyjson_doc_free(doc);
}

// free the buffer
free(buf);
```

If you are not sure about the amount of memory required to process JSON, you can use the dynamic allocator.
```c
// create a dynamic allocator
yyjson_alc *alc = yyjson_alc_dyn_new();

// read multiple JSON using one allocator
for(int i = 0, i < your_json_file_count; i++) {
    const char *your_json_file_path = ...;
    yyjson_doc *doc = yyjson_read_file(your_json_file_path, 0, alc, NULL);
    ...
    yyjson_doc_free(doc);
}

// free the allocator
yyjson_alc_dyn_free(alc);
```



## Stack memory allocator
If the JSON is small enough, you can use stack memory to read or write it.

Sample code:
```c
char buf[128 * 1024]; // stack buffer
yyjson_alc alc;
yyjson_alc_pool_init(&alc, buf, sizeof(buf));

yyjson_doc *doc = yyjson_read_opts(dat, len, 0, &alc, NULL);
...
yyjson_doc_free(doc); // this is optional, as the memory is on stack
```

## Use a third-party allocator library
You can use a third-party high-performance memory allocator for yyjson, such as [jemalloc](https://github.com/jemalloc/jemalloc), [tcmalloc](https://github.com/google/tcmalloc), [mimalloc](https://github.com/microsoft/mimalloc). You can also refer to the following code to implement your own allocator.

Sample code:
```c
// Use https://github.com/microsoft/mimalloc

#include <mimalloc.h>

// same as malloc(size)
static void *priv_malloc(void *ctx, size_t size) {
    return mi_malloc(size);
}

// same as realloc(ptr, size)
// `old_size` is the size of the originally allocated memory
static void *priv_realloc(void *ctx, void *ptr, size_t old_size, size_t size) {
    return mi_realloc(ptr, size);
}

// same as free(ptr)
static void priv_free(void *ctx, void *ptr) {
    mi_free(ptr);
}

// the allocator object
static const yyjson_alc PRIV_ALC = {
    priv_malloc,
    priv_realloc,
    priv_free,
    NULL // `ctx` which will be passed into the functions above
};

// Read with custom allocator
yyjson_doc *doc = yyjson_doc_read_opts(dat, len, 0, &PRIV_ALC, NULL);
...
yyjson_doc_free(doc);

// Write with custom allocator
yyjson_alc *alc = &PRIV_ALC;
char *json = yyjson_doc_write(doc, 0, alc, NULL, NULL);
...
alc->free(alc->ctx, json);

```



# Stack Memory Usage
Most functions in the library use fixed-size stack memory. This includes functions for JSON reading and writing, as well as JSON Pointer handling.

However, a few functions use recursion and may cause a stack overflow if the object level is too deep. These functions are marked with the following warning in the header file: 
> @warning 
> This function is recursive and may cause a stack overflow 
> if the object level is too deep.



# Null Check
The library's public APIs perform a `null check` for every input parameter to prevent crashes.

For example, when reading a JSON, you don't need to perform null checks or type checks on each value:
```c
yyjson_doc *doc = yyjson_read(NULL, 0, 0); // doc is NULL
yyjson_val *val = yyjson_doc_get_root(doc); // val is NULL
const char *str = yyjson_get_str(val); // str is NULL
if (!str) printf("err!");
yyjson_doc_free(doc); // do nothing
```

However, if you are certain that a value is non-null and matches the expected type, you can use the `unsafe` prefix API to avoid the null check.

For example, when iterating over an array or object, the value and key must be non-null:
```c
size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    // this is a valid JSON, so the key must be a valid string
    if (unsafe_yyjson_equals_str(key, "id") &&
        unsafe_yyjson_is_uint(val) &&
        unsafe_yyjson_get_uint(val) == 1234) {
        ...
    }
}
```



# Thread Safety
The library does not use global variables. Therefore, if you can ensure that the input parameters of a function are thread-safe, then the function calls are also thread-safe.<br/>

In general, `yyjson_doc` and `yyjson_val` are immutable and thread-safe, while `yyjson_mut_doc` and `yyjson_mut_val` are mutable and not thread-safe.



# Locale Independence
The library is designed to be locale-independent.

However, there are certain conditions that you should be aware of:

1. You use libc's `setlocale()` function to change the locale.
2. Your environment does not adhere the IEEE 754 floating-point standard (e.g. some IBM mainframes), or you explicitly set `YYJSON_DISABLE_FAST_FP_CONV` during build, in such case yyjson will use `strtod()` to parse floating-point numbers.

If **both** of these conditions are met, it is recommended to avoid calling `setlocale()` while another thread is parsing JSON. Otherwise, an error may be returned during JSON floating-point number parsing.
