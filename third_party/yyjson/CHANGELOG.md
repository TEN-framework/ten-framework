# Changelog
All notable changes to this project will be documented in this file.


## 0.10.0 (2024-07-09)
#### Added
- Add `yyjson_locate_pos()` function to locate the line and column number for error position: #166

#### Changed
- Improve error messages for JSON reader: #168

#### Fixed
- Fix `YYJSON_READ_NUMBER_AS_RAW` not overriding `YYJSON_READ_BIGNUM_AS_RAW` as per documentation: #170


## 0.9.0 (2024-04-08)
#### Added
- Add `YYJSON_WRITE_NEWLINE_AT_END` flag for JSON writer: #147

#### Changed
- Add auto-type conversion (uint<->sint) to `yyjson_ptr_get_uint/sint()`: #152

#### Fixed
- Fix incorrect output in environments lacking native `bool` type support: #161


## 0.8.0 (2023-09-13)
#### Added
- Add `YYJSON_SUBTYPE_NOESC` subtype to mark strings that do not need to be escaped.
- Add `YYJSON_DISABLE_UTF8_VALIDATION` flag to allow disable UTF-8 validation at compile-time.
- Add dynamic allocator API: `yyjson_alc_dyn_new()`, `yyjson_alc_dyn_free()`.
- Add the missing `yyjson_mut_obj_add_arr/obj()` API: #140

#### Changed
- Improve the write performance of strings with `YYJSON_SUBTYPE_NOESC`.

#### Fixed
- Fix clang-16 valgrind fail: #134
- Fix compile break when both `FAST_FP` and `READER` are disabled


## 0.7.0 (2023-05-25)
#### Added
- Add `YYJSON_WRITE_PRETTY_TWO_SPACES` option to allow 2 spaces instead of 4 spaces when writing pretty JSON: #99
- Add `YYJSON_READ_BIGNUM_AS_RAW` option to read big numbers as raw strings: #124
- Add `yyjson_get_num()` function to convert and return any number value as `double`: #108
- Add support for Loongarch: #112
- Add functions to get type-specific values specified by JSON Pointer: #116
- Add functions to read/write JSON with file pointer `FILE *`: #122
- Add functions to support modifying memory pool size of `yyjson_mut_doc`.
- Add convenience functions `iter_with()` for creating iterator.
- Add functions to modify JSON using JSON Pointer, such as `ptr_set()` and `ptr_remove()`.
- Add support for JSON Patch (RFC 6902).

#### Changed
- **BREAKING CHANGE:** Change the allocator's realloc function signature, add `old_size` parameter for custom allocator: #100
- **BREAKING CHANGE:** Change `yyjson_read_number()` function, add `alc` parameter.
- **DEPRECATED:** Deprecate `get_pointer()` functions, rename to `ptr_get()`.
- Improve performance of `yyjson_mut_write()` function.

#### Fixed
- Fix inaccurate error code for truncated JSON: #103


## 0.6.0 (2022-12-12)
#### Added
- Add functions to modify the content of a JSON value, such as `yyjson_set_int(yyjson_val *val, int num)`.
- Add functions to copy from mutable doc to immutable doc.
- Add functions to support renaming an object's key.
- Add the `yyjson_read_number()` function to parse numeric strings.
- Add a placeholder allocator if `yyjson_alc_pool_init()` fails.

#### Fixed
- Fix quite NaN on MIPS and HPPA arch.
- Fixed compile error before `GCC 4.5`, which doesn't support empty optional extended asm label.
- When the built-in floating point conversion is disabled, the `sprintf()` output for floating point numbers is missing a decimal point, for example 123 should be 123.0.


## 0.5.1 (2022-06-17)
#### Fixed
- Fix run-time error when compiling as cpp and 32-bit (g++-5 -m32 -fPIC) #85
- Fix incurrect output number format, remove unnecessary digits (e.g. 2.0e34 -> 2e34).


## 0.5.0 (2022-05-25)
#### Added
- Add LibFuzzer support.
- Add Doxygen support.
- Add functions to support serializing a single JSON value.
- Add `yyjson_mut_doc_mut_copy()`, `yyjson_mut_val_mut_copy()`, `yyjson_mut_merge_patch()` functions for mutable input.
- Add `yyjson_equals()` and `yyjson_mut_equals()` functions to compare two values.
- Add `yyjson_mut_obj_remove_key()` and `yyjson_mut_obj_remove_keyn()` functions to simplify key removal.
- Add `YYJSON_READ_NUMBER_AS_RAW` option and `RAW` type support.
- Add `YYJSON_READ_ALLOW_INVALID_UNICODE` and `YYJSON_WRITE_ALLOW_INVALID_UNICODE` options to allow invalid unicode.

#### Changed
- Change `yyjson_mut_obj_remove()` return type from `bool` to `yyjson_mut_val *`.
- Rewrite string serialization function, validate unicode encoding by default.
- Rewrite the JSON Pointer implementation, remove internal malloc() calls.

#### Fixed
- Make the code work correctly with `setlocale()` function and `-ffast-math` flag: #54
- Fix negative infinity literals read error: #64
- Fix non null-terminated string write error.
- Fix incorrect behavior of `YYJSON_DISABLE_NON_STANDARD` flag: #80


## 0.4.0 (2021-12-12)
#### Added
- Add `YYJSON_WRITE_INF_AND_NAN_AS_NULL` flag for JSON writer.
- Add `yyjson_merge_patch()` function for JSON Merge-Path API (RFC 7386).
- Add `yyjson_mut_obj_replace()` and `yyjson_mut_obj_insert()` functions for object modification.
- Add `yyjson_obj_iter_get()` and `yyjson_mut_obj_iter_get()` functions for faster object search.
- Add `yyjson_version()` function.

#### Changed
- Replace `YYJSON_DISABLE_COMMENT_READER` and `YYJSON_DISABLE_INF_AND_NAN_READER` with `YYJSON_DISABLE_NON_STANDARD` compile-time flag.
- Replace `YYJSON_DISABLE_FP_READER` and `YYJSON_DISABLE_FP_WRITER` with `YYJSON_DISABLE_FAST_FP_CONV` compile-time flag.

#### Fixed
- Fix compiler warning with `-Wconversion`
- Fix compiler error for GCC 4.4 (#53) and MSVC 6.0 (#55)


## 0.3.0 (2021-05-25)
#### Added
- Add `JSON Pointer` support.
- Add CMake install target.

#### Changed
- Improve performance for some architectures that don't support unaligned memory access.

#### Fixed
- Fix some compiler warnings for GCC and Clang.
- Fix MSVC build error on UWP (uninitialized local variable).
- Fix stream file reading error on some platforms.


## 0.2.0 (2020-12-12)
#### Added
- Add swift package manager support.

#### Changed
- Improve JSON reader performance for gcc.
- Improve double number reader performance with a fast path.
- Rewrite double number writer with Schubfach algorithm: #4.
- Strict UTF-8 validation for JSON reader.

#### Removed
- Remove `YYJSON_READ_FASTFP` compile-time flag.

#### Fixed
- Fix a compile error for old version gcc on linux: #7.


## 0.1.0 (2020-10-26)
#### Added
- Initial release.
- Add the basic JSON reader and writer (RFC 8259).
- Add CMake support.
- Add GitHub CI workflow.
- Add test code and test data.
- Add `sanitizer` and `valgrind` memory checker.
- Add `API` and `DataStructure` documentation.
