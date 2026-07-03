Alternative implementations of Mbed TLS functionality
=====================================================

This document describes how parts of the Mbed TLS functionality can be replaced at compile time to integrate the library on a platform.

This document is an overview. It is not exhaustive. Please consult the documentation of individual modules and read the library header files for more details.

## Platform integration

Mbed TLS works out of the box on Unix/Linux/POSIX-like systems and on Windows. On embedded platforms, you may need to customize some aspects of how Mbed TLS interacts with the underlying platform. This section discusses the main areas that can be configured.

The platform module (`include/mbedtls/platform.h`) controls how Mbed TLS accesses standard library features such as memory management (`calloc`, `free`), `printf`, `exit`. You can define custom functions instead of the ones from the C standard library through `MBEDTLS_PLATFORM_XXX` options in the configuration file. Many options have two mechanisms: either define `MBEDTLS_PLATFORM_XXX_MACRO` to the name of a function to call instead of the standard function `xxx`, or define `MBEDTLS_PLATFORM_XXX_ALT` and [register an alternative implementation during the platform setup](#alternative-implementations-of-platform-functions).

The storage of the non-volatile seed for random generation, enabled with `MBEDTLS_ENTROPY_NV_SEED`, is also controlled via the platform module.

For timing functions, you can [declare an alternative implementation of the timing module](#module-alternative-implementations).

On multithreaded platforms, [declare an alternative implementation of the threading module](#module-alternative-implementations).

To configure entropy sources (hardware random generators), see the `MBEDTLS_ENTROPY_XXX` options in the configuration file.

For networking, the `net_sockets` module does not currently support alternative implementations. If this module does not work on your platform, disable `MBEDTLS_NET_C` and use custom functions for TLS.

If your platform has a cryptographic accelerator, you can use it via a [PSA driver](#psa-cryptography-drivers) or declare an [alternative implementation of the corresponding module(s)](#module-alternative-implementations) or [of specific functions](#function-alternative-implementations). PSA drivers will ultimately replace the alternative implementation mechanism, but alternative implementation will remain supported in at least all Mbed TLS versions of the form 3.x. The interface of PSA drivers is currently still experimental and subject to change.

## PSA cryptography drivers

On platforms where a hardware cryptographic engine is present, you can implement a driver for this engine in the PSA interface. Drivers are supported for cryptographic operations with transparent keys (keys available in cleartext), for cryptographic operations with opaque keys (keys that are only available inside the cryptographic engine), and for random generation. Calls to `psa_xxx` functions that perform cryptographic operations are directed to drivers instead of the built-in code as applicable. See the [PSA cryptography driver interface specification](docs/proposed/psa-driver-interface.md), the [Mbed TLS PSA driver developer guide](docs/proposed/psa-driver-developer-guide.md) and the [Mbed TLS PSA driver integration guide](docs/proposed/psa-driver-integration-guide.md) for more information.

As of Mbed TLS 3.0, this interface is still experimental and subject to change, and not all operations support drivers yet. The configuration option `MBEDTLS_USE_PSA_CRYPTO` causes parts of the `mbedtls_xxx` API to use PSA crypto and therefore to support drivers, however it is not yet compatible with all drivers.

## Module alternative implementations

You can replace the code of some modules of Mbed TLS at compile time by a custom implementation. This is possible for low-level cryptography modules (symmetric algorithms, DHM, RSA, ECP, ECJPAKE) and for some platform-related modules (threading, timing). Such custom implementations are called “alternative implementations”, or “ALT implementations” for short.

The general principle of an alternative implementation is:
* Enable `MBEDTLS_XXX_ALT` in the compile-time configuration where XXX is the module name. For example, `MBEDTLS_AES_ALT` for an implementation of the AES module. This is in addition to enabling `MBEDTLS_XXX_C`.
* Create a header file `xxx_alt.h` that defines the context type(s) used by the module. For example, `mbedtls_aes_context` for AES.
* Implement all the functions from the module, i.e. the functions declared in `include/mbedtls/xxx.h`.

See https://tls.mbed.org/kb/development/hw_acc_guidelines for a more detailed guide.

### Constraints on context types

Generally, alternative implementations can define their context types to any C type except incomplete and array types (although they would normally be `struct` types). This section lists some known limitations where the context type needs to be a structure with certain fields.

Where a context type needs to have a certain field, the field must have the same type and semantics as in the built-in implementation, but does not need to be at the same position in the structure. Furthermore, unless otherwise indicated, only read access is necessary: the field can be `const`, and modifications to it do not need to be supported. For example, if an alternative implementation of asymmetric cryptography uses a different representation of large integers, it is sufficient to provide a read-only copy of the fields listed here of type `mbedtls_mpi`.

* AES: if `MBEDTLS_AESNI_C` or `MBEDTLS_PADLOCK_C` is enabled, `mbedtls_aes_context` must have the fields `nr` and `rk`.
* DHM: if `MBEDTLS_DEBUG_C` is enabled, `mbedtls_dhm_context` must have the fields `P`, `Q`, `G`, `GX`, `GY` and `K`.
* ECP: `mbedtls_ecp_group` must have the fields `id`, `P`, `A`, `B`, `G`, `N`, `pbits` and `nbits`.
    * If `MBEDTLS_PK_PARSE_EC_EXTENDED` is enabled, those fields must be writable, and `mbedtls_ecp_point_read_binary()` must support a group structure where only `P`, `pbits`, `A` and `B` are set.

It must be possible to move a context object in memory (except during the execution of a library function that takes this context as an argument). (This is necessary, for example, to support applications that populate a context on the stack of an inner function and then copy the context upwards through the call chain, or applications written in a language with automatic memory management that can move objects on the heap.) That is, call sequences like the following must work:
```
mbedtls_xxx_context ctx1, ctx2;
mbedtls_xxx_init(&ctx1);
mbedtls_xxx_setup(&ctx1, …);
ctx2 = ctx1;
memset(&ctx1, 0, sizeof(ctx1));
mbedtls_xxx_do_stuff(&ctx2, …);
mbedtls_xxx_free(&ctx2);
```
In practice, this means that a pointer to a context or to a part of a context does not remain valid across function calls. Alternative implementations do not need to support copying of contexts: contexts can only be cloned through explicit `clone()` functions.

## Function alternative implementations

In some cases, it is possible to replace a single function or a small set of functions instead of [providing an alternative implementation of the whole module](#module-alternative-implementations).

### Alternative implementations of cryptographic functions

Options to replace individual functions of cryptographic modules generally have a name obtained by upper-casing the function name and appending `_ALT`. If the function name contains `_internal`, `_ext` or `_ret`, this is removed in the `_ALT` symbol. When the corresponding option is enabled, the built-in implementation of the function will not be compiled, and you must provide an alternative implementation at link time.

For example, enable `MBEDTLS_AES_ENCRYPT_ALT` at compile time and provide your own implementation of `mbedtls_aes_encrypt()` to provide an accelerated implementation of AES encryption that is compatible with the built-in key schedule. If you wish to implement key schedule differently, you can also enable `MBEDTLS_AES_SETKEY_ENC_ALT` and implement `mbedtls_aes_setkey_enc()`.

Another example: enable `MBEDTLS_SHA256_PROCESS_ALT` and implement `mbedtls_internal_sha256_process()` to provide an accelerated implementation of SHA-256 and SHA-224.

Note that since alternative implementations of individual functions cooperate with the built-in implementation of other functions, you must use the same layout for context objects as the built-in implementation. If you want to use different context types, you need to [provide an alternative implementation of the whole module](#module-alternative-implementations).

### Alternative implementations of platform functions

Several platform functions can be reconfigured dynamically by following the process described here. To reconfigure how Mbed TLS calls the standard library function `xxx()`:

* Define the symbol `MBEDTLS_PLATFORM_XXX_ALT` at compile time.
* During the initialization of your application, set the global variable `mbedtls_xxx` to an alternative implementation of `xxx()`.

For example, to provide a custom `printf` function at run time, enable `MBEDTLS_PLATFORM_PRINTF_ALT` at compile time and assign to `mbedtls_printf` during the initialization of your application.

Merely enabling `MBEDTLS_PLATFORM_XXX_ALT` does not change the behavior: by default, `mbedtls_xxx` points to the standard function `xxx`.

Note that there are variations on the naming pattern. For example, some configurable functions are activated in pairs, such as `mbedtls_calloc` and `mbedtls_free` via `MBEDTLS_PLATFORM_MEMORY`. Consult the documentation of individual configuration options and of the platform module for details.
