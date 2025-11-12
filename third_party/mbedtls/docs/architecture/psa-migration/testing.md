Testing strategy for `MBEDTLS_USE_PSA_CRYPTO`
=============================================

This document records the testing strategy used so far in implementing
`MBEDTLS_USE_PSA_CRYPTO`.


General considerations
----------------------

There needs to be at least one build in `all.sh` that enables
`MBEDTLS_USE_PSA_CRYPTO` and runs the full battery of tests; currently that's
ensured by the fact that `scripts/config.py full` enables
`MBEDTLS_USE_PSA_CRYPTO`. There needs to be at least one build with
`MBEDTLS_USE_PSA_CRYPTO` disabled (as long as it's optional); currently that's
ensured by the fact that it's disabled in the default config.

Generally, code review is enough to ensure that PSA APIs are indeed used where
they should be when `MBEDTLS_USE_PSA_CRYPTO` is enabled.

However, when it comes to TLS, we also have the option of using debug messages
to confirm which code path is taken. This is generally unnecessary, except when
a decision is made at run-time about whether to use the PSA or legacy code
path. (For example, for record protection, previously (until 3.1), some ciphers were supported
via PSA while some others weren't, with a run-time fallback. In this case, it's
good to have a debug message checked by the test case to confirm that the
right decision was made at run-time, i. e. that we didn't use the fallback for
ciphers that are supposed to be supported.)


New APIs meant for application use
----------------------------------

For example, `mbedtls_pk_setup_opaque()` is meant to be used by applications
in order to create PK contexts that can then be passed to existing TLS and
X.509 APIs (which remain unchanged).

In that case, we want:

- unit testing of the new API and directly-related APIs - for example:
  - in `test_suite_pk` we have a new test function `pk_psa_utils` that exercises
    `mbedtls_pk_setup_opaque()` and checks that various utility functions
  (`mbedtls_pk_get_type()` etc.) work and the functions that are expected to
  fail (`mbedtls_pk_verify()` etc) return the expected error.
  - in `test_suite_pk` we modified the existing `pk_psa_sign` test function to
    check that signature generation works as expected
  - in `test_suite_pkwrite` we should have a new test function checking that
    exporting (writing out) the public part of the key works as expected and
    that exporting the private key fails as expected.
- integration testing of the new API with each existing API which should
  accepts a context created this way - for example:
  - in `programs/ssl/ssl_client2` a new option `key_opaque` that causes the
    new API to be used, and one or more tests in `ssl-opt.sh` using that.
    (We should have the same server-side.)
  - in `test_suite_x509write` we have a new test function
    `x509_csr_check_opaque()` checking integration of the new API with the
    existing `mbedtls_x509write_csr_set_key()`. (And also
    `mbedtls_x509write_crt_set_issuer_key()` since #5710.)

For some APIs, for example with `mbedtls_ssl_conf_psk_opaque()`, testing in
`test_suite_ssl` was historically not possible, so we only have testing in
`ssl-opt.sh`.

New APIs meant for internal use
-------------------------------

For example, `mbedtls_cipher_setup_psa()` (no longer used, soon to be
deprecated - #5261) was meant to be used by the TLS layer, but probably not
directly by applications.

In that case, we want:

- unit testing of the new API and directly-related APIs - for example:
  - in `test_suite_cipher`, the existing test functions `auth_crypt_tv` and
    `test_vec_crypt` gained a new parameter `use_psa` and corresponding test
    cases
- integration testing:
  - usually already covered by existing tests for higher-level modules:
    - for example simple use of `mbedtls_cipher_setup_psa()` in TLS is already
      covered by running the existing TLS tests in a build with
      `MBEDTLS_USA_PSA_CRYPTO` enabled
  - however if use of the new API in higher layers involves more logic that
    use of the old API, specific integrations test may be required
    - for example, the logic to fall back from `mbedtls_cipher_setup_psa()` to
      `mbedtls_cipher_setup()` in TLS is tested by `run_test_psa` in
      `ssl-opt.sh`.

Internal changes
----------------

For example, use of PSA to compute the TLS 1.2 PRF.

Changes in this category rarely require specific testing, as everything should
be already be covered by running the existing tests in a build with
`MBEDTLS_USE_PSA_CRYPTO` enabled; however we need to make sure the existing
test have sufficient coverage, and improve them if necessary.

However, if additional logic is involved, or there are run-time decisions about
whether to use the PSA or legacy code paths, specific tests might be in order.
