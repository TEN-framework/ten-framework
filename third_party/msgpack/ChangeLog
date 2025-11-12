# 2023-03-02 version 6.0.0
  * Remove C++ requirement if test is disabled (#1055)
## << breaking changes >>
  * Change CMake package name of C library to msgpack-c (#1053)
    Unified all C package, library, cmake, tarball name become msgpack-c.

# 2023-01-10 version 5.0.0
  * Add additional address sanitizer for CI. (#1023)
## << breaking changes >>
  * Change CMake package name of C library to msgpackc (#1044, #1049)

# 2021-08-01 version 4.0.0
  * Fix and improve alignment logic (#962)
  * Fix iovec name conflict (#953)
  * Fix empty string print (#942)
  * Fix buffer ptr size (#899)
  * Fix UB. Check null pointer before using memcpy() (#890)
  * Improve CI environment (#885, #899)

## << breaking changes >>
* Separate C part of the msgpack-c from C/C++ mixed msgpack-c (#876, #878)


# 2020-06-05 version 3.3.0
  * Add json example for C (#870)
  * Add both header and body packing functions for C (#870)
  * Set default ref_size and chunk_size to vrefbuffer (#865)
  * Add examples (#861)
  * Improve build system (#839, #842)
  * Improve tests (#829)
  * Improve documents (#828)
  * Remove some warnings (#827, #851, #871)
  * Improve CI environment (#824, #831, #833, #834, #846, #860, 874)

# 2019-12-10 version 3.2.1
  * Fix snprintf return value checking (#821)
  * Remove some warnings (#819)
  * Fix fbuffer result checking (#812)
  * Fix temporary object handling (#807)
  * Improve cmake support (#804)
  * Fix invalid `int main` parameter (#800)
  * Improve supporting platform (#797, #817)
  * Fix ZLIB error handling (#795)
  * Remove unused variable (#793)
  * Improve integer overflow checking (#792)

# 2019-05-27 version 3.2.0

  * Fix invalid include (#783)
  * Add timespec support (#781)
  * Fix unchecked fnprintf on C (#780)
  * Improve integer overflow checking on C (#776)
  * Fix warnings on `-Wconversion` (#770, #777, #784)
  * Fix invalid passed by value on aligned_zone_size_visitor (#764)
  * Improve windows support (#757, #779)
  * Fix msgpack::object size caluclation error (#754)
  * Fix memory error on example code (#753)
  * Fix redundant memory allocation on C (#747)
  * Fix msgpack::type::tuple base class conversion (#743)

# 2018-09-09 version 3.1.1

  * Add force endian set functionality (#736)
  * Fix vrefbuffer memory management problem (#733)
  * Fix msvc specific problem (#731, #732)
  * Update boost from 1.61.0 to 1.68.0 (#730)
  * Fix msgpack_timestamp type mismatch bug (#726)

# 2018-08-10 version 3.1.0

  * Improve documents (#687, #718)
  * Add fuzzer support (#689)
  * Fix msgpack::object union member access bug (#694)
  * Improve cross platform configuration (#704)
  * Fix out of range dereference bug of EXT (#705)
  * Add timestamp support. std::chrono::system_clock::time_point is mapped to TIMESTAMP (#706)
  * Add minimal timestamp support for C. The type `msgpack_timestamp` and the function `msgpack_object_to_timestamp()` are introduced (#707)
  * Improve MSGPACK_DEFINE family name confliction probability (#710)
  * Add no static-library build option (BUILD_SHARED_LIBS=ON) (#713, #717, #722)
  * Add header only cmake target (#721)
  * Add `std::byte` adaptor (#719)
  * Remove some warnings (#720)

# 2018-05-12 version 3.0.1

  * Add fuzz directory to release tar ball (#686)
  * Add include file checking for X-Code (#683)

# 2018-05-09 version 3.0.0

## << breaking changes >>

  * Change offset parameter updating rule. If parse error happens, offset is updated to the error position. (#639, #666)

## << other updates >>

  * Improve cross platform configuration (#655, #677)
  * Improve build system (#647)
  * Improve user class adaptor (#645, #673)
  * Improve msgpack::object visitation logic (#676)
  * Remove some warnings (#641, 659)
  * Add `->` and `*` operators to object_handle (#635)
  * Improve CI environment (#631, #634, #643, #657, #662, #668)
  * Improve documents (#630, #661)
  * Refactoring (#670)
  * Add OSS-Fuzz support (#672, #674, #675, #678)

# 2017-08-04 version 2.1.5
  * Improve cross platform configuration (#624)
  * Add boost asio examples (including zlib) (#610)
  * Remove some warnings (#611)
  * Fix unpack visitor to treat float32/64 correctly (#613)
  * Improve documents (#616)
  * Fix alignment problem on some platform (#617, #518)
  * Fix conflict std::tuple, std::pair, and boost::fusion::sequence problem (#619)

# 2017-08-03 version 2.1.4 (Invalid)
  * See https://github.com/msgpack/msgpack-c/issues/623

# 2017-06-15 version 2.1.3
  * Improve build system (#603)
  * Add C++17 adaptors `std::optional` and `std::string_view`. (#607, #608)
  * Improve cross platform configuration (#601)
  * Remove some warnings (#599, #602, #605)

# 2017-06-07 version 2.1.2

* Improve documents (#565)
  * Fix empty map parse bug (#568)
  * Improve build system (#569, #570, #572, #579, #591, #592)
  * Remove some warnings (#574, #578, #586, #588)
  * Improve cross platform configuration (#577, #582)
  * Add cmake package config support (#580)
  * Fix streaming unpack bug (#585)

# 2017-02-04 version 2.1.1

  * Fix unpacker's buffer management bug (#561)
  * Add boost string_view adaptor (#558)
  * Remove some warnings (#557, #559)
  * Improve coding style (#556)

# 2017-01-10 version 2.1.0

## << breaking changes >>

  * Fix object internal data type is float if msgpack format is float32 (#531)

## << recommended changes >>

  * Add `FLOAT64` type. Please use it instead of `DOUBLE` (#531)
  * Add `FLOAT32` type. Please use it instead of `FLOAT` (#531)

## << other updates >>

  * Add iterator based parse/unpack function(experimental) (#553)
  * Add `[[deprecated]]` attribute for C++14 (#552)
  * Fix `msgpack_unpack()` return code (#548)
  * Fix integer overflow (#547, #549, #550)
  * Add example codes (#542)
  * Add MSGPACK_NVP. You can use not only variable name but also any strings (#535)
  * Fix and Improve build system (#532, #545)
  * Fix `gcc_atomic.hpp` include path (#529, #530)
  * Improve CI environment (#526)
  * Improve documents (#524)
  * Add msgpack_unpacker_next_with_size() function (#515)
  * Fix `as()` applying condition (#511)
  * Fix fbuffer write (#504)
  * Add gcc bug workaround (#499)
  * Improve object print (#497, #500, #505, #533)
  * Remove some warnings (#495, #506, #508, #513, #528, #538, #545)

# 2016-06-25 version 2.0.0

## << breaking changes >>

  * Removed autotools support. Use cmake instead (#476, #479)
  * Removed pointer version of msgpack::unpack APIs. Use reference version instead (#453)
  * Removed MSGPACK_DISABLE_LEGACY_CONVERT. msgpack::object::convert(T*) is removed by default. Use msgpack::object::convert(T&) instead (#451)
  * Removed msgpacl::type::nil. Use nil_t or define MSGPACK_USE_LECACY_NIL (#444)
  * Removed std::string to msgpack::object conversion (#434)

## << recommended changes >>

  * Replaced msgpack::unpacked with msgpack::object_handle. msgpack::unpacked is kept as a typedef of msgpack::object_handle. (#448)

## << other updates >>

  * Add strict size checking adaptor. Relaxed tuple conversion (#489)
  * Fix and Improve example codes (#487)
  * Add C++/CLI support for nullptr (#481)
  * Update the boost libraries that are contained by msgpack-c (#475)
  * Fix gcc_atomic.hpp location (#474)
  * Add C-Style array support (#466, #488)
  * Fix include file dependency (#464)
  * Add a visitor version of unpack API (#461)
  * Fix JSON string conversion from "nil" to "null" (#458)
  * Fix and Improve build system (#455, #471, #473, #486, #491)
  * Fix comments (#452)
  * Fix unintentional msgpack::zone moving problem (#447)
  * Fix operator>> and << for msgpack::object (#443)
  * Fix C++03 msgpack::zone::clear() memory access violation bug (#441)
  * Fix TARGET_OS_IPHONE checking (#436)
  * Fix invalid front() call for empty container (#435)
  * Fix compile error on g++6 (C++11 only) (#426, #430)
  * Fix zone size expansion logic (#423)
  * Fix wrong hader file dependency (#421)
  * Fix msvc specific problem (#420)
  * Add v2 API support (#415)

# 2016-01-22 version 1.4.0

## << recommended changes >>

  * Define [MSGPACK_DISABLE_LEGACY_NIL](https://github.com/msgpack/msgpack-c/wiki/v1_1_cpp_configure#msgpack_disable_legacy_nil-since-140), then `msgpack::type::nil` is replaced by with `msgpack::type::nil_t` (#408, #411, #412).
    Replace `msgpack::type::nil` with `msgpack::type::nil_t` in client codes.
    `msgpack::type::nil` will be removed on the version 2.0.0.
  * Define [MSGPACK_DISABLE_LEGACY_CONVERT](https://github.com/msgpack/msgpack-c/wiki/v1_1_cpp_configure#msgpack_disable_legacy_convert-since-140), then `msgpack::object::convert(T*)` is removed (#410).
    Replace calling `msgpack::bojectconvert(T*)` with `msgpack::bojectconvert(T&)` in client codes as follows:

    ```C++
     int i;
     obj.convert(&i); // before
     ```

    ```C++
     int i;
     obj.convert(i);  // after
     ```

    `msgpack::object::convert(T*)` will be removed on the version 2.0.0.

Define the macros above as follows when you compile C++ codes that use msgpack-c:

```
g++ -Ipath_to_msgpack/include -DMSGPACK_DISABLE_LEGACY_NIL -DMSGPACK_DISABLE_LEGACY_CONVERT your_code.cpp

```

You can compile existing codes without defining macros above but I recommend defining them and updating your codes to fix the issues #408, #411, #412, #399, and #410. It is also a good preparation for the version 2.0.0.

## << other updates >>

  * Improve documents (#387, #407)
  * Remove C++ version library (#394, #402)
  * Add Doxyfile and ChangeLog to the distribution package (#397)
  * Add signed/unsigned char test to travis-ci (#398)
  * Remove some warnings (#400, #401, #409)
  * Fix endian checking. (#404)

# 2015-11-21 version 1.3.0

  * Change the license from the Apache License Version 2.0 to the
    Boost Software License, Version 1.0.(#386)
  * Remove some warnings (#365)
  * Add std::reference_wrapper support(#373, #384)
  * Improve tests (#375, #378, #379, #380)
  * Fix msvc specific problem (#376, #383)
  * Fix typos (#381)

# 2015-09-04 version 1.2.0

## << breaking changes >>

  * Change std::vector<unsigned char> and std::array<unsigned char>
    mapped to BIN instead of ARRAY (#243)
  * Remove redundant copy (#285)


## << other updates >>

  * Add array_ref to map to ARRAY (#243)
  * Add variant type and adaptor (#349)
  * Add object::convert_if_not_nil() (#357)
  * Fix invalid offset update (#354)
  * Add C++11 support on MSVC2015(#339, #347)
  * Fix and Improve build system (#346, #350, #361, #363)
  * Import Boost.Preprocessor as a part of msgpack-c (#312)
  * Fix OSX with libc++ specific errors (#334, #362)
  * Add customized containers support (#330)
  * Add std::unique_ptr and std::shared_ptr support (#329)
  * Add missing install files (#328)
  * Add shared/static library switching option (#316)
  * Improve no throw description on C++11 (#313)
  * Import Boost.Predef as a part of msgpack-c (#312)
  * Add map based serialize support (#306)
  * Add Boost.Fusion support (#305)
  * Add v4 format RAW support (#304)
  * Fix zbuffer with empty string problem (#303)
  * Add non default constructible class support (#302, #324, #327, #331, #332, #345)
  * Add inline keyword to function (template) (#299)
  * Add EXT type supporting classes (#292, #308)
  * Fix raw_ref != comparison (#290)
  * Add object deep copy (#288)
  * Remove some warnings (#284, #322, #323, #335)
  * Improve compiler version checking (#283)
  * Add return value to object::convert() (#282)
  * Improve move semantic support in C++11 (#279, #353)
  * Add Boost.StringRef support (#278)
  * Improve CI environment (#276, #294, #338)
  * Add converting to JSON (#274, #301)
  * Fix iOS specific problem (#270)
  * Improve doxtgen document generation (#269)
  * Add Boost.Optional support (#268)
  * Fix msvc specific problem (#267, #295)
  * Add base class serialization. (#265, #277)
  * Add and improve examples. (#264, #310, #311, #341, #342, #344)
  * Fix wiki URL. (#263)

# 2015-04-03 version 1.1.0

## << breaking changes >>

  * Remove msgpack_fwd.hpp
  * Improve user types adaptation mechanism (#262)
    Since version 1.0.0, users need to obey the correct include order.
    However, it is very difficult to maintain the correct order in big
    projects. version 1.1.0 removed this order. Users don't need to
    care about include order. Migration guide from 1.0.x to 1.1.0 has
    been written. See https://github.com/msgpack/msgpack-c/wiki


## << other updates >>

  * Fix vector<bool> size check (#251)
  * Fix inttypes.h inclusion on MSVC (#257)
  * Support documents generation by Doxygen (#259)
  * Remove C99 style variable declaration (#253)
  * Improve documents (https://github.com/msgpack/msgpack-c/wiki)

# 2015-03-22 version 1.0.1:

  * Fix compilation error on Mac 10.9 (#244)
  * Fix typos in documents (#240)
  * Update CHANGELOG.md for version 1.0.0 (#242)
  * Fix erb templates for the next code generation (#239)

# 2015-03-10 version 1.0.0:

  * Support msgpack v5 format (str, bin, and ext) https://github.com/msgpack/msgpack/blob/master/spec.md (#142)
  * Support std::tuple, std::forward_list, std::array, std::unordered_set, std::unordered_map on C++11. tr1 unordered containers are still supported (#53, #130, #137, #154, #169)
  * Update msgpack-c as a header-only library on C++ (#142)
  * Move include directory (#142)
  * Update the name of float format family on msgpack::object from 'dec' to 'f64' (#194)
  * Remove existing elements on associative containers when unpacking (#127)
  * Add an API versioning functionality https://github.com/msgpack/msgpack-c/wiki/cpp_versioning (#139)
  * Add C++11 enum class support (#205)
  * Map std::vector<char> and std::array<char> to BIN (#100)
  * Map '\0' teminated char* and char const* to STR (#206)
  * Add the new parameter on unpacking functions and classes to limit msgpack's bytestream size (https://github.com/msgpack/msgpack-c/wiki/cpp_unpacker#limit-size-of-elements) (#175)
  * Add the copy or reference choosing function on unpack() and unpacker (https://github.com/msgpack/msgpack-c/wiki/cpp_unpacker#memory-management)
  * Add the new unpack() overloads for C++11 https://github.com/msgpack/msgpack-c/wiki/cpp_unpacker (#128)
  * Add a msgpack::object::with_zone (deep) copying function (#133, #163)
  * Remove the compile-time defined limit of msgpack nest level on C++ (#218)
  * Add the new unpack() overloads that use an existing zone (#201)
  * Add the range-based for loop support on msgpack object array and map (#203)
  * Add msgpack revision getter function for 'revision' (#237)
  * Support EXT for C (#118, #129)
  * Fix unpacking buffer allocation problem when malformed data is given (#160, #185)
  * Add dll exporting function on MSVC (#162)
  * Fix msgpack::zone::allocate_no_align(). Now it allocates the memory that is not aligned as expected (#171)
  * Improve documents (https://github.com/msgpack/msgpack-c/wiki)
  * Other bug fixes and refactoring: #62, #91, #95, #97, #107, #109, #113, #117, #119, #121, #122, #123, #126, #131, #136, #138, #140, #143, #145, #146, #150, #151, #152, #156, #157, #158, #161, #165, #170, #172, #179, #180, #181, #182, #183, #192, #195, #199, #200, #207, #211, #212, #219, #222, #224, #230, #231, #232, #233, #234, #235

# 2014-07-02 version 0.5.9:

  * Support std::tr1 unordered containers by default (#51, #63, #68, #69)
  * Remove some warnings (#56)
  * Fix segmentation fault after malloc failures (#58, #59)
  * Fix alloc/dealloc mismatch (#52, #61)
  * Fix sample codes (#60, #64)
  * Support implicit conversion from integer to float/double (#54)
  * Improve documents (#45, #75, #82, #83)
  * Support CMake (#20, #87)
  * Remove Ruby dependencies in bootstrap (#86, #87)
  * Add FILE* buffer (#40)
  * Other bug fixes and refactoring: #39, #73, #77, #79, #80, #81, #84, #90

# 2013-12-23 version 0.5.8:

  * Move to the new github repository msgpack/msgpack-c
  * Support the new deserialization specification
  * fixes the problem of unpack helpers for array and map with 32bit compilers (#37, #38)
  * Other bug fixes and refactoring: #46, #41, #36, #35, #33, #32, #30, #29, #28, #27, #26, #25, #8, #3
  * Update of documents: #23, #18, #17

# 2011-08-08 version 0.5.7:

  * fixes compile error problem with llvm-gcc and Mac OS X Lion

# 2011-04-24 version 0.5.6:

  * #42 fixes double-free problem on msgpack_unpacker_release_zone

# 2011-02-24 version 0.5.5:

  * eliminates dependency of winsock2.h header
  * fixes msgpack_vc.postbuild.bat file
  * fixes some implicit cast warnings

# 2010-08-29 version 0.5.4:

  * includes msgpack_vc2008.vcproj file in source package
  * fixes type::fix_int types

# 2010-08-27 version 0.5.3:

  * adds type::fix_{u,}int{8,16,32,64} types
  * adds msgpack_pack_fix_{u,}int{8,16,32,64} functions
  * adds packer<Stream>::pack_fix_{u,}int{8,16,32,64} functions
  * fixes include paths

# 2010-07-14 version 0.5.2:

  * type::raw::str(), operator==, operator!=, operator< and operator> are now const
  * generates version.h using AC_OUTPUT macro in ./configure

# 2010-07-06 version 0.5.1:

  * Add msgpack_vrefbuffer_new and msgpack_vrefbuffer_free
  * Add msgpack_sbuffer_new and msgpack_sbuffer_free
  * Add msgpack_unpacker_next and msgpack_unpack_next
  * msgpack::unpack returns void
  * Add MSGPACK_VERSION{,_MAJOR,_MINOR} macros to check header version
  * Add msgpack_version{,_major,_minor} functions to check library version
  * ./configure supports --disable-cxx option not to build C++ API

# 2010-04-29 version 0.5.0:

  * msgpack_object_type is changed. MSGPACK_OBJECT_NIL is now 0x00.
  * New safe streaming deserializer API.
  * Add object::object(const T&) and object::operator=(const T&)
  * Add operator==(object, const T&)
  * MSGPACK_DEFINE macro defines msgpack_object(object* obj, zone* z)
  * C++ programs doesn't need to link "msgpackc" library.
