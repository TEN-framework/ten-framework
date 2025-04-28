## Getting started with Mbed Crypto

### What is Mbed Crypto?

Mbed Crypto is an open source cryptographic library that supports a wide range of cryptographic operations, including:
* Key management
* Hashing
* Symmetric cryptography
* Asymmetric cryptography
* Message authentication (MAC)
* Key generation and derivation
* Authenticated encryption with associated data (AEAD)

The Mbed Crypto library is a reference implementation of the cryptography interface of the Arm Platform Security Architecture (PSA). It is written in portable C.

The Mbed Crypto library is distributed under the Apache License, version 2.0.

#### Platform Security Architecture (PSA)

Arm's Platform Security Architecture (PSA) is a holistic set of threat models,
security analyses, hardware and firmware architecture specifications, and an open source firmware reference implementation. PSA provides a recipe, based on industry best practice, that enables you to design security into both hardware and firmware consistently. Part of the API provided by PSA is the cryptography interface, which provides access to a set of primitives.

### Using Mbed Crypto

* [Getting the Mbed Crypto library](#getting-the-mbed-crypto-library)
* [Building the Mbed Crypto library](#building-the-mbed-crypto-library)
* [Using the Mbed Crypto library](#using-the-mbed-crypto-library)
* [Importing a key](#importing-a-key)
* [Signing a message using RSA](#signing-a-message-using-RSA)
* [Encrypting or decrypting using symmetric ciphers](#encrypting-or-decrypting-using-symmetric-ciphers)
* [Hashing a message](#hashing-a-message)
* [Deriving a new key from an existing key](#deriving-a-new-key-from-an-existing-key)
* [Generating a random value](#generating-a-random-value)
* [Authenticating and encrypting or decrypting a message](#authenticating-and-encrypting-or-decrypting-a-message)
* [Generating and exporting keys](#generating-and-exporting-keys)
* [More about the Mbed Crypto library](#more-about-the-mbed-crypto-library)

### Getting the Mbed Crypto library

Mbed Crypto releases are available in the [public GitHub repository](https://github.com/ARMmbed/mbed-crypto).

### Building the Mbed Crypto library

**Prerequisites to building the library with the provided makefiles:**
* GNU Make.
* A C toolchain (compiler, linker, archiver).
* Python 2 or Python 3 (either works) to generate the test code.
* Perl to run the tests.

If you have a C compiler such as GCC or Clang, just run `make` in the top-level directory to build the library, a set of unit tests and some sample programs.

To select a different compiler, set the `CC` variable to the name or path of the compiler and linker (default: `cc`) and set `AR` to a compatible archiver (default: `ar`); for example:
```
make CC=arm-linux-gnueabi-gcc AR=arm-linux-gnueabi-ar
```
The provided makefiles pass options to the compiler that assume a GCC-like command line syntax. To use a different compiler, you may need to pass different values for `CFLAGS`, `WARNINGS_CFLAGS` and `LDFLAGS`.

To run the unit tests on the host machine, run `make test` from the top-level directory. If you are cross-compiling, copy the test executable from the `tests` directory to the target machine.

### Using the Mbed Crypto library

To use the Mbed Crypto APIs, call `psa_crypto_init()` before calling any other API. This initializes the library.

### Importing a key

To use a key for cryptography operations in Mbed Crypto, you need to first
import it. The import operation returns the identifier of the key for use
with other function calls.

**Prerequisites to importing keys:**
* Initialize the library with a successful call to `psa_crypto_init()`.

This example shows how to import a key:
```C
void import_a_key(const uint8_t *key, size_t key_len)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;

    printf("Import an AES key...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Set key attributes */
    psa_set_key_usage_flags(&attributes, 0);
    psa_set_key_algorithm(&attributes, 0);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);

    /* Import the key */
    status = psa_import_key(&attributes, key, key_len, &key_id);
    if (status != PSA_SUCCESS) {
        printf("Failed to import key\n");
        return;
    }
    printf("Imported a key\n");

    /* Free the attributes */
    psa_reset_key_attributes(&attributes);

    /* Destroy the key */
    psa_destroy_key(key_id);

    mbedtls_psa_crypto_free();
}
```

### Signing a message using RSA

Mbed Crypto supports encrypting, decrypting, signing and verifying messages using public key signature algorithms, such as RSA or ECDSA.

**Prerequisites to performing asymmetric signature operations:**
* Initialize the library with a successful call to `psa_crypto_init()`.
* Have a valid key with appropriate attributes set:
    * Usage flag `PSA_KEY_USAGE_SIGN_HASH` to allow signing.
    * Usage flag `PSA_KEY_USAGE_VERIFY_HASH` to allow signature verification.
    * Algorithm set to the desired signature algorithm.

This example shows how to sign a hash that has already been calculated:
```C
void sign_a_message_using_rsa(const uint8_t *key, size_t key_len)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    uint8_t hash[32] = {0x50, 0xd8, 0x58, 0xe0, 0x98, 0x5e, 0xcc, 0x7f,
                        0x60, 0x41, 0x8a, 0xaf, 0x0c, 0xc5, 0xab, 0x58,
                        0x7f, 0x42, 0xc2, 0x57, 0x0a, 0x88, 0x40, 0x95,
                        0xa9, 0xe8, 0xcc, 0xac, 0xd0, 0xf6, 0x54, 0x5c};
    uint8_t signature[PSA_SIGNATURE_MAX_SIZE] = {0};
    size_t signature_length;
    psa_key_id_t key_id;

    printf("Sign a message...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Set key attributes */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attributes, PSA_ALG_RSA_PKCS1V15_SIGN_RAW);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);
    psa_set_key_bits(&attributes, 1024);

    /* Import the key */
    status = psa_import_key(&attributes, key, key_len, &key_id);
    if (status != PSA_SUCCESS) {
        printf("Failed to import key\n");
        return;
    }

    /* Sign message using the key */
    status = psa_sign_hash(key_id, PSA_ALG_RSA_PKCS1V15_SIGN_RAW,
                           hash, sizeof(hash),
                           signature, sizeof(signature),
                           &signature_length);
    if (status != PSA_SUCCESS) {
        printf("Failed to sign\n");
        return;
    }

    printf("Signed a message\n");

    /* Free the attributes */
    psa_reset_key_attributes(&attributes);

    /* Destroy the key */
    psa_destroy_key(key_id);

    mbedtls_psa_crypto_free();
}
```

### Using symmetric ciphers

Mbed Crypto supports encrypting and decrypting messages using various symmetric cipher algorithms (both block and stream ciphers).

**Prerequisites to working with the symmetric cipher API:**
* Initialize the library with a successful call to `psa_crypto_init()`.
* Have a symmetric key. This key's usage flags must include `PSA_KEY_USAGE_ENCRYPT` to allow encryption or `PSA_KEY_USAGE_DECRYPT` to allow decryption.

**To encrypt a message with a symmetric cipher:**
1. Allocate an operation (`psa_cipher_operation_t`) structure to pass to the cipher functions.
1. Initialize the operation structure to zero or to `PSA_CIPHER_OPERATION_INIT`.
1. Call `psa_cipher_encrypt_setup()` to specify the algorithm and the key to be used.
1. Call either `psa_cipher_generate_iv()` or `psa_cipher_set_iv()` to generate or set the initialization vector (IV). We recommend calling `psa_cipher_generate_iv()`, unless you require a specific IV value.
1. Call `psa_cipher_update()` with the message to encrypt. You may call this function multiple times, passing successive fragments of the message on successive calls.
1. Call `psa_cipher_finish()` to end the operation and output the encrypted message.

This example shows how to encrypt data using an AES (Advanced Encryption Standard) key in CBC (Cipher Block Chaining) mode with no padding (assuming all prerequisites have been fulfilled):
```c
void encrypt_with_symmetric_ciphers(const uint8_t *key, size_t key_len)
{
    enum {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
    };
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_algorithm_t alg = PSA_ALG_CBC_NO_PADDING;
    uint8_t plaintext[block_size] = SOME_PLAINTEXT;
    uint8_t iv[block_size];
    size_t iv_len;
    uint8_t output[block_size];
    size_t output_len;
    psa_key_id_t key_id;
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

    printf("Encrypt with cipher...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Import a key */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    status = psa_import_key(&attributes, key, key_len, &key_id);
    if (status != PSA_SUCCESS) {
        printf("Failed to import a key\n");
        return;
    }
    psa_reset_key_attributes(&attributes);

    /* Encrypt the plaintext */
    status = psa_cipher_encrypt_setup(&operation, key_id, alg);
    if (status != PSA_SUCCESS) {
        printf("Failed to begin cipher operation\n");
        return;
    }
    status = psa_cipher_generate_iv(&operation, iv, sizeof(iv), &iv_len);
    if (status != PSA_SUCCESS) {
        printf("Failed to generate IV\n");
        return;
    }
    status = psa_cipher_update(&operation, plaintext, sizeof(plaintext),
                               output, sizeof(output), &output_len);
    if (status != PSA_SUCCESS) {
        printf("Failed to update cipher operation\n");
        return;
    }
    status = psa_cipher_finish(&operation, output + output_len,
                               sizeof(output) - output_len, &output_len);
    if (status != PSA_SUCCESS) {
        printf("Failed to finish cipher operation\n");
        return;
    }
    printf("Encrypted plaintext\n");

    /* Clean up cipher operation context */
    psa_cipher_abort(&operation);

    /* Destroy the key */
    psa_destroy_key(key_id);

    mbedtls_psa_crypto_free();
}
```

**To decrypt a message with a symmetric cipher:**
1. Allocate an operation (`psa_cipher_operation_t`) structure to pass to the cipher functions.
1. Initialize the operation structure to zero or to `PSA_CIPHER_OPERATION_INIT`.
1. Call `psa_cipher_decrypt_setup()` to specify the algorithm and the key to be used.
1. Call `psa_cipher_set_iv()` with the IV for the decryption.
1. Call `psa_cipher_update()` with the message to encrypt. You may call this function multiple times, passing successive fragments of the message on successive calls.
1. Call `psa_cipher_finish()` to end the operation and output the decrypted message.

This example shows how to decrypt encrypted data using an AES key in CBC mode with no padding
(assuming all prerequisites have been fulfilled):
```c
void decrypt_with_symmetric_ciphers(const uint8_t *key, size_t key_len)
{
    enum {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
    };
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_algorithm_t alg = PSA_ALG_CBC_NO_PADDING;
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    uint8_t ciphertext[block_size] = SOME_CIPHERTEXT;
    uint8_t iv[block_size] = ENCRYPTED_WITH_IV;
    uint8_t output[block_size];
    size_t output_len;
    psa_key_id_t key_id;

    printf("Decrypt with cipher...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Import a key */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    status = psa_import_key(&attributes, key, key_len, &key_id);
    if (status != PSA_SUCCESS) {
        printf("Failed to import a key\n");
        return;
    }
    psa_reset_key_attributes(&attributes);

    /* Decrypt the ciphertext */
    status = psa_cipher_decrypt_setup(&operation, key_id, alg);
    if (status != PSA_SUCCESS) {
        printf("Failed to begin cipher operation\n");
        return;
    }
    status = psa_cipher_set_iv(&operation, iv, sizeof(iv));
    if (status != PSA_SUCCESS) {
        printf("Failed to set IV\n");
        return;
    }
    status = psa_cipher_update(&operation, ciphertext, sizeof(ciphertext),
                               output, sizeof(output), &output_len);
    if (status != PSA_SUCCESS) {
        printf("Failed to update cipher operation\n");
        return;
    }
    status = psa_cipher_finish(&operation, output + output_len,
                               sizeof(output) - output_len, &output_len);
    if (status != PSA_SUCCESS) {
        printf("Failed to finish cipher operation\n");
        return;
    }
    printf("Decrypted ciphertext\n");

    /* Clean up cipher operation context */
    psa_cipher_abort(&operation);

    /* Destroy the key */
    psa_destroy_key(key_id);

    mbedtls_psa_crypto_free();
}
```

#### Handling cipher operation contexts

After you've initialized the operation structure with a successful call to `psa_cipher_encrypt_setup()` or `psa_cipher_decrypt_setup()`, you can terminate the operation at any time by calling `psa_cipher_abort()`.

The call to `psa_cipher_abort()` frees any resources associated with the operation, except for the operation structure itself.

Mbed Crypto implicitly calls `psa_cipher_abort()` when:
* A call to `psa_cipher_generate_iv()`, `psa_cipher_set_iv()` or `psa_cipher_update()` fails (returning any status other than `PSA_SUCCESS`).
* A call to `psa_cipher_finish()` succeeds or fails.

After an implicit or explicit call to `psa_cipher_abort()`, the operation structure is invalidated; in other words, you cannot reuse the operation structure for the same operation. You can, however, reuse the operation structure for a different operation by calling either `psa_cipher_encrypt_setup()` or `psa_cipher_decrypt_setup()` again.

You must call `psa_cipher_abort()` at some point for any operation that is initialized successfully (by a successful call to `psa_cipher_encrypt_setup()` or `psa_cipher_decrypt_setup()`).

Making multiple sequential calls to `psa_cipher_abort()` on an operation that is terminated (either implicitly or explicitly) is safe and has no effect.

### Hashing a message

Mbed Crypto lets you compute and verify hashes using various hashing
algorithms.

**Prerequisites to working with the hash APIs:**
* Initialize the library with a successful call to `psa_crypto_init()`.

**To calculate a hash:**
1. Allocate an operation structure (`psa_hash_operation_t`) to pass to the hash functions.
1. Initialize the operation structure to zero or to `PSA_HASH_OPERATION_INIT`.
1. Call `psa_hash_setup()` to specify the hash algorithm.
1. Call `psa_hash_update()` with the message to encrypt. You may call this function multiple times, passing successive fragments of the message on successive calls.
1. Call `psa_hash_finish()` to calculate the hash, or `psa_hash_verify()` to compare the computed hash with an expected hash value.

This example shows how to calculate the SHA-256 hash of a message:
```c
    psa_status_t status;
    psa_algorithm_t alg = PSA_ALG_SHA_256;
    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    unsigned char input[] = { 'a', 'b', 'c' };
    unsigned char actual_hash[PSA_HASH_MAX_SIZE];
    size_t actual_hash_len;

    printf("Hash a message...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Compute hash of message  */
    status = psa_hash_setup(&operation, alg);
    if (status != PSA_SUCCESS) {
        printf("Failed to begin hash operation\n");
        return;
    }
    status = psa_hash_update(&operation, input, sizeof(input));
    if (status != PSA_SUCCESS) {
        printf("Failed to update hash operation\n");
        return;
    }
    status = psa_hash_finish(&operation, actual_hash, sizeof(actual_hash),
                             &actual_hash_len);
    if (status != PSA_SUCCESS) {
        printf("Failed to finish hash operation\n");
        return;
    }

    printf("Hashed a message\n");

    /* Clean up hash operation context */
    psa_hash_abort(&operation);

    mbedtls_psa_crypto_free();
```

This example shows how to verify the SHA-256 hash of a message:
```c
    psa_status_t status;
    psa_algorithm_t alg = PSA_ALG_SHA_256;
    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    unsigned char input[] = { 'a', 'b', 'c' };
    unsigned char expected_hash[] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde,
        0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    size_t expected_hash_len = PSA_HASH_LENGTH(alg);

    printf("Verify a hash...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Verify message hash */
    status = psa_hash_setup(&operation, alg);
    if (status != PSA_SUCCESS) {
        printf("Failed to begin hash operation\n");
        return;
    }
    status = psa_hash_update(&operation, input, sizeof(input));
    if (status != PSA_SUCCESS) {
        printf("Failed to update hash operation\n");
        return;
    }
    status = psa_hash_verify(&operation, expected_hash, expected_hash_len);
    if (status != PSA_SUCCESS) {
        printf("Failed to verify hash\n");
        return;
    }

    printf("Verified a hash\n");

    /* Clean up hash operation context */
    psa_hash_abort(&operation);

    mbedtls_psa_crypto_free();
```

The API provides the macro `PSA_HASH_LENGTH`, which returns the expected hash length (in bytes) for the specified algorithm.

#### Handling hash operation contexts

After a successful call to `psa_hash_setup()`, you can terminate the operation at any time by calling `psa_hash_abort()`. The call to `psa_hash_abort()` frees any resources associated with the operation, except for the operation structure itself.

Mbed Crypto implicitly calls `psa_hash_abort()` when:
1. A call to `psa_hash_update()` fails (returning any status other than `PSA_SUCCESS`).
1. A call to `psa_hash_finish()` succeeds or fails.
1. A call to `psa_hash_verify()` succeeds or fails.

After an implicit or explicit call to `psa_hash_abort()`, the operation structure is invalidated; in other words, you cannot reuse the operation structure for the same operation. You can, however, reuse the operation structure for a different operation by calling `psa_hash_setup()` again.

You must call `psa_hash_abort()` at some point for any operation that is initialized successfully (by a successful call to `psa_hash_setup()`) .

Making multiple sequential calls to `psa_hash_abort()` on an operation that has already been terminated (either implicitly or explicitly) is safe and has no effect.

### Generating a random value

Mbed Crypto can generate random data.

**Prerequisites to generating random data:**
* Initialize the library with a successful call to `psa_crypto_init()`.

<span class="notes">**Note:** To generate a random key, use `psa_generate_key()` instead of `psa_generate_random()`.</span>

This example shows how to generate ten bytes of random data by calling `psa_generate_random()`:
```C
    psa_status_t status;
    uint8_t random[10] = { 0 };

    printf("Generate random...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    status = psa_generate_random(random, sizeof(random));
    if (status != PSA_SUCCESS) {
        printf("Failed to generate a random value\n");
        return;
    }

    printf("Generated random data\n");

    /* Clean up */
    mbedtls_psa_crypto_free();
```

### Deriving a new key from an existing key

Mbed Crypto provides a key derivation API that lets you derive new keys from
existing ones. The key derivation API has functions to take inputs, including
other keys and data, and functions to generate outputs, such as new keys or
other data.

You must first initialize and set up a key derivation context,
provided with a key and, optionally, other data. Then, use the key derivation context to either read derived data to a buffer or send derived data directly to a key slot.

See the documentation for the particular algorithm (such as HKDF or the TLS1.2 PRF) for
information about which inputs to pass when, and when you can obtain which outputs.

**Prerequisites to working with the key derivation APIs:**
* Initialize the library with a successful call to `psa_crypto_init()`.
* Use a key with the appropriate attributes set:
    * Usage flags set for key derivation (`PSA_KEY_USAGE_DERIVE`)
    * Key type set to `PSA_KEY_TYPE_DERIVE`.
    * Algorithm set to a key derivation algorithm
      (for example, `PSA_ALG_HKDF(PSA_ALG_SHA_256)`).

**To derive a new AES-CTR 128-bit encryption key into a given key slot using HKDF
with a given key, salt and info:**

1. Set up the key derivation context using the `psa_key_derivation_setup()`
function, specifying the derivation algorithm `PSA_ALG_HKDF(PSA_ALG_SHA_256)`.
1. Provide an optional salt with `psa_key_derivation_input_bytes()`.
1. Provide info with `psa_key_derivation_input_bytes()`.
1. Provide a secret with `psa_key_derivation_input_key()`, referencing a key that
   can be used for key derivation.
1. Set the key attributes desired for the new derived key. We'll set
   the `PSA_KEY_USAGE_ENCRYPT` usage flag and the `PSA_ALG_CTR` algorithm for this
   example.
1. Derive the key by calling `psa_key_derivation_output_key()`.
1. Clean up the key derivation context.

At this point, the derived key slot holds a new 128-bit AES-CTR encryption key
derived from the key, salt and info provided:
```C
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    static const unsigned char key[] = {
        0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b };
    static const unsigned char salt[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c };
    static const unsigned char info[] = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
        0xf7, 0xf8, 0xf9 };
    psa_algorithm_t alg = PSA_ALG_HKDF(PSA_ALG_SHA_256);
    psa_key_derivation_operation_t operation =
        PSA_KEY_DERIVATION_OPERATION_INIT;
    size_t derived_bits = 128;
    size_t capacity = PSA_BITS_TO_BYTES(derived_bits);
    psa_key_id_t base_key;
    psa_key_id_t derived_key;

    printf("Derive a key (HKDF)...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Import a key for use in key derivation. If such a key has already been
     * generated or imported, you can skip this part. */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_DERIVE);
    status = psa_import_key(&attributes, key, sizeof(key), &base_key);
    if (status != PSA_SUCCESS) {
        printf("Failed to import a key\n");
        return;
    }
    psa_reset_key_attributes(&attributes);

    /* Derive a key */
    status = psa_key_derivation_setup(&operation, alg);
    if (status != PSA_SUCCESS) {
        printf("Failed to begin key derivation\n");
        return;
    }
    status = psa_key_derivation_set_capacity(&operation, capacity);
    if (status != PSA_SUCCESS) {
        printf("Failed to set capacity\n");
        return;
    }
    status = psa_key_derivation_input_bytes(&operation,
                                            PSA_KEY_DERIVATION_INPUT_SALT,
                                            salt, sizeof(salt));
    if (status != PSA_SUCCESS) {
        printf("Failed to input salt (extract)\n");
        return;
    }
    status = psa_key_derivation_input_key(&operation,
                                          PSA_KEY_DERIVATION_INPUT_SECRET,
                                          base_key);
    if (status != PSA_SUCCESS) {
        printf("Failed to input key (extract)\n");
        return;
    }
    status = psa_key_derivation_input_bytes(&operation,
                                            PSA_KEY_DERIVATION_INPUT_INFO,
                                            info, sizeof(info));
    if (status != PSA_SUCCESS) {
        printf("Failed to input info (expand)\n");
        return;
    }
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CTR);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    status = psa_key_derivation_output_key(&attributes, &operation,
                                           &derived_key);
    if (status != PSA_SUCCESS) {
        printf("Failed to derive key\n");
        return;
    }
    psa_reset_key_attributes(&attributes);

    printf("Derived key\n");

    /* Clean up key derivation operation */
    psa_key_derivation_abort(&operation);

    /* Destroy the keys */
    psa_destroy_key(derived_key);
    psa_destroy_key(base_key);

    mbedtls_psa_crypto_free();
```

### Authenticating and encrypting or decrypting a message

Mbed Crypto provides a simple way to authenticate and encrypt with associated data (AEAD), supporting the `PSA_ALG_CCM` algorithm.

**Prerequisites to working with the AEAD cipher APIs:**
* Initialize the library with a successful call to `psa_crypto_init()`.
* The key attributes for the key used for derivation must have the `PSA_KEY_USAGE_ENCRYPT` or `PSA_KEY_USAGE_DECRYPT` usage flags.

This example shows how to authenticate and encrypt a message:
```C
    psa_status_t status;
    static const uint8_t key[] = {
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF };
    static const uint8_t nonce[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B };
    static const uint8_t additional_data[] = {
        0xEC, 0x46, 0xBB, 0x63, 0xB0, 0x25,
        0x20, 0xC3, 0x3C, 0x49, 0xFD, 0x70 };
    static const uint8_t input_data[] = {
        0xB9, 0x6B, 0x49, 0xE2, 0x1D, 0x62, 0x17, 0x41,
        0x63, 0x28, 0x75, 0xDB, 0x7F, 0x6C, 0x92, 0x43,
        0xD2, 0xD7, 0xC2 };
    uint8_t *output_data = NULL;
    size_t output_size = 0;
    size_t output_length = 0;
    size_t tag_length = 16;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;

    printf("Authenticate encrypt...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    output_size = sizeof(input_data) + tag_length;
    output_data = (uint8_t *)malloc(output_size);
    if (!output_data) {
        printf("Out of memory\n");
        return;
    }

    /* Import a key */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    status = psa_import_key(&attributes, key, sizeof(key), &key_id);
    psa_reset_key_attributes(&attributes);

    /* Authenticate and encrypt */
    status = psa_aead_encrypt(key_id, PSA_ALG_CCM,
                              nonce, sizeof(nonce),
                              additional_data, sizeof(additional_data),
                              input_data, sizeof(input_data),
                              output_data, output_size,
                              &output_length);
    if (status != PSA_SUCCESS) {
        printf("Failed to authenticate and encrypt\n");
        return;
    }

    printf("Authenticated and encrypted\n");

    /* Clean up */
    free(output_data);

    /* Destroy the key */
    psa_destroy_key(key_id);

    mbedtls_psa_crypto_free();
```

This example shows how to authenticate and decrypt a message:

```C
    psa_status_t status;
    static const uint8_t key_data[] = {
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF };
    static const uint8_t nonce[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B };
    static const uint8_t additional_data[] = {
        0xEC, 0x46, 0xBB, 0x63, 0xB0, 0x25,
        0x20, 0xC3, 0x3C, 0x49, 0xFD, 0x70 };
    static const uint8_t input_data[] = {
        0x20, 0x30, 0xE0, 0x36, 0xED, 0x09, 0xA0, 0x45, 0xAF, 0x3C, 0xBA, 0xEE,
        0x0F, 0xC8, 0x48, 0xAF, 0xCD, 0x89, 0x54, 0xF4, 0xF6, 0x3F, 0x28, 0x9A,
        0xA1, 0xDD, 0xB2, 0xB8, 0x09, 0xCD, 0x7C, 0xE1, 0x46, 0xE9, 0x98 };
    uint8_t *output_data = NULL;
    size_t output_size = 0;
    size_t output_length = 0;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;

    printf("Authenticate decrypt...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    output_size = sizeof(input_data);
    output_data = (uint8_t *)malloc(output_size);
    if (!output_data) {
        printf("Out of memory\n");
        return;
    }

    /* Import a key */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);
    status = psa_import_key(&attributes, key_data, sizeof(key_data), &key_id);
    if (status != PSA_SUCCESS) {
        printf("Failed to import a key\n");
        return;
    }
    psa_reset_key_attributes(&attributes);

    /* Authenticate and decrypt */
    status = psa_aead_decrypt(key_id, PSA_ALG_CCM,
                              nonce, sizeof(nonce),
                              additional_data, sizeof(additional_data),
                              input_data, sizeof(input_data),
                              output_data, output_size,
                              &output_length);
    if (status != PSA_SUCCESS) {
        printf("Failed to authenticate and decrypt %ld\n", status);
        return;
    }

    printf("Authenticated and decrypted\n");

    /* Clean up */
    free(output_data);

    /* Destroy the key */
    psa_destroy_key(key_id);

    mbedtls_psa_crypto_free();
```

### Generating and exporting keys

Mbed Crypto provides a simple way to generate a key or key pair.

**Prerequisites to using key generation and export APIs:**
* Initialize the library with a successful call to `psa_crypto_init()`.

**To generate an ECDSA key:**
1. Set the desired key attributes for key generation by calling
   `psa_set_key_algorithm()` with the chosen ECDSA algorithm (such as
   `PSA_ALG_DETERMINISTIC_ECDSA(PSA_ALG_SHA_256)`). You only want to export the public key, not the key pair (or private key); therefore, do not set `PSA_KEY_USAGE_EXPORT`.
1. Generate a key by calling `psa_generate_key()`.
1. Export the generated public key by calling `psa_export_public_key()`:
```C
    enum {
        key_bits = 256,
    };
    psa_status_t status;
    size_t exported_length = 0;
    static uint8_t exported[PSA_KEY_EXPORT_ECC_PUBLIC_KEY_MAX_SIZE(key_bits)];
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id;

    printf("Generate a key pair...\t");
    fflush(stdout);

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Failed to initialize PSA Crypto\n");
        return;
    }

    /* Generate a key */
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attributes,
                          PSA_ALG_DETERMINISTIC_ECDSA(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes,
                     PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, key_bits);
    status = psa_generate_key(&attributes, &key_id);
    if (status != PSA_SUCCESS) {
        printf("Failed to generate key\n");
        return;
    }
    psa_reset_key_attributes(&attributes);

    status = psa_export_public_key(key_id, exported, sizeof(exported),
                                   &exported_length);
    if (status != PSA_SUCCESS) {
        printf("Failed to export public key %ld\n", status);
        return;
    }

    printf("Exported a public key\n");

    /* Destroy the key */
    psa_destroy_key(key_id);

    mbedtls_psa_crypto_free();
```

### More about the PSA Crypto API

For more information about the PSA Crypto API, please see the [PSA Cryptography API Specification](https://armmbed.github.io/mbed-crypto/html/index.html).
