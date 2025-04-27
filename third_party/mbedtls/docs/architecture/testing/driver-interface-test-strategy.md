# Mbed Crypto driver interface test strategy

This document describes the test strategy for the driver interfaces in Mbed Crypto. Mbed Crypto has interfaces for secure element drivers, accelerator drivers and entropy drivers. This document is about testing Mbed Crypto itself; testing drivers is out of scope.

The driver interfaces are standardized through PSA Cryptography functional specifications.

## Secure element driver interface testing

### Secure element driver interfaces

#### Opaque driver interface

The [unified driver interface](../../proposed/psa-driver-interface.md) supports both transparent drivers (for accelerators) and opaque drivers (for secure elements).

Drivers exposing this interface need to be registered at compile time by declaring their JSON description file.

#### Dynamic secure element driver interface

The dynamic secure element driver interface (SE interface for short) is defined by [`psa/crypto_se_driver.h`](../../../include/psa/crypto_se_driver.h). This is an interface between Mbed Crypto and one or more third-party drivers.

The SE interface consists of one function provided by Mbed Crypto (`psa_register_se_driver`) and many functions that drivers must implement. To make a driver usable by Mbed Crypto, the initialization code must call `psa_register_se_driver` with a structure that describes the driver. The structure mostly contains function pointers, pointing to the driver's methods. All calls to a driver function are triggered by a call to a PSA crypto API function.

### SE driver interface unit tests

This section describes unit tests that must be implemented to validate the secure element driver interface. Note that a test case may cover multiple requirements; for example a “good case” test can validate that the proper function is called, that it receives the expected inputs and that it produces the expected outputs.

Many SE driver interface unit tests could be covered by running the existing API tests with a key in a secure element.

#### SE driver registration

This applies to dynamic drivers only.

* Test `psa_register_se_driver` with valid and with invalid arguments.
* Make at least one failing call to `psa_register_se_driver` followed by a successful call.
* Make at least one test that successfully registers the maximum number of drivers and fails to register one more.

#### Dispatch to SE driver

For each API function that can lead to a driver call (more precisely, for each driver method call site, but this is practically equivalent):

* Make at least one test with a key in a secure element that checks that the driver method is called. A few API functions involve multiple driver methods; these should validate that all the expected driver methods are called.
* Make at least one test with a key that is not in a secure element that checks that the driver method is not called.
* Make at least one test with a key in a secure element with a driver that does not have the requisite method (i.e. the method pointer is `NULL`) but has the substructure containing that method, and check that the return value is `PSA_ERROR_NOT_SUPPORTED`.
* Make at least one test with a key in a secure element with a driver that does not have the substructure containing that method (i.e. the pointer to the substructure is `NULL`), and check that the return value is `PSA_ERROR_NOT_SUPPORTED`.
* At least one test should register multiple drivers with a key in each driver and check that the expected driver is called. This does not need to be done for all operations (use a white-box approach to determine if operations may use different code paths to choose the driver).
* At least one test should register the same driver structure with multiple lifetime values and check that the driver receives the expected lifetime value.

Some methods only make sense as a group (for example a driver that provides the MAC methods must provide all or none). In those cases, test with all of them null and none of them null.

#### SE driver inputs

For each API function that can lead to a driver call (more precisely, for each driver method call site, but this is practically equivalent):

* Wherever the specification guarantees parameters that satisfy certain preconditions, check these preconditions whenever practical.
* If the API function can take parameters that are invalid and must not reach the driver, call the API function with such parameters and verify that the driver method is not called.
* Check that the expected inputs reach the driver. This may be implicit in a test that checks the outputs if the only realistic way to obtain the correct outputs is to start from the expected inputs (as is often the case for cryptographic material, but not for metadata).

#### SE driver outputs

For each API function that leads to a driver call, call it with parameters that cause a driver to be invoked and check how Mbed Crypto handles the outputs.

* Correct outputs.
* Incorrect outputs such as an invalid output length.
* Expected errors (e.g. `PSA_ERROR_INVALID_SIGNATURE` from a signature verification method).
* Unexpected errors. At least test that if the driver returns `PSA_ERROR_GENERIC_ERROR`, this is propagated correctly.

Key creation functions invoke multiple methods and need more complex error handling:

* Check the consequence of errors detected at each stage (slot number allocation or validation, key creation method, storage accesses).
* Check that the storage ends up in the expected state. At least make sure that no intermediate file remains after a failure.

#### Persistence of SE keys

The following tests must be performed at least one for each key creation method (import, generate, ...).

* Test that keys in a secure element survive `psa_close_key(); psa_open_key()`.
* Test that keys in a secure element survive `mbedtls_psa_crypto_free(); psa_crypto_init()`.
* Test that the driver's persistent data survives `mbedtls_psa_crypto_free(); psa_crypto_init()`.
* Test that `psa_destroy_key()` does not leave any trace of the key.

#### Resilience for SE drivers

Creating or removing a key in a secure element involves multiple storage modifications (M<sub>1</sub>, ..., M<sub>n</sub>). If the operation is interrupted by a reset at any point, it must be either rolled back or completed.

* For each potential interruption point (before M<sub>1</sub>, between M<sub>1</sub> and M<sub>2</sub>, ..., after M<sub>n</sub>), call `mbedtls_psa_crypto_free(); psa_crypto_init()` at that point and check that this either rolls back or completes the operation that was started.
* This must be done for each key creation method and for key destruction.
* This must be done for each possible flow, including error cases (e.g. a key creation that fails midway due to `OUT_OF_MEMORY`).
* The recovery during `psa_crypto_init` can itself be interrupted. Test those interruptions too.
* Two things need to be tested: the key that is being created or destroyed, and the driver's persistent storage.
* Check both that the storage has the expected content (this can be done by e.g. using a key that is supposed to be present) and does not have any unexpected content (for keys, this can be done by checking that `psa_open_key` fails with `PSA_ERROR_DOES_NOT_EXIST`).

This requires instrumenting the storage implementation, either to force it to fail at each point or to record successive storage states and replay each of them. Each `psa_its_xxx` function call is assumed to be atomic.

### SE driver system tests

#### Real-world use case

We must have at least one driver that is close to real-world conditions:

* With its own source tree.
* Running on actual hardware.
* Run the full driver validation test suite (which does not yet exist).
* Run at least one test application (e.g. the Mbed OS TLS example).

This requirement shall be fulfilled by the [Microchip ATECC508A driver](https://github.com/ARMmbed/mbed-os-atecc608a/).

#### Complete driver

We should have at least one driver that covers the whole interface:

* With its own source tree.
* Implementing all the methods.
* Run the full driver validation test suite (which does not yet exist).

A PKCS#11 driver would be a good candidate. It would be useful as part of our product offering.

## Transparent driver interface testing

The [unified driver interface](../../proposed/psa-driver-interface.md) defines interfaces for accelerators.

### Test requirements

#### Requirements for transparent driver testing

Every cryptographic mechanism for which a transparent driver interface exists (key creation, cryptographic operations, …) must be exercised in at least one build. The test must verify that the driver code is called.

#### Requirements for fallback

The driver interface includes a fallback mechanism so that a driver can reject a request at runtime and let another driver handle the request. For each entry point, there must be at least three test runs with two or more drivers available with driver A configured to fall back to driver B, with one run where A returns `PSA_SUCCESS`, one where A returns `PSA_ERROR_NOT_SUPPORTED` and B is invoked, and one where A returns a different error and B is not invoked.

## Entropy and randomness interface testing

TODO
