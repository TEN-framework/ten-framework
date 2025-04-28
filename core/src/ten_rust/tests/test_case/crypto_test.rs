//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use ten_rust::crypto::{self, CipherAlgorithm};

#[test]
fn test_aes_ctr_crypto() {
    let origin_str = "hello world! this is my plaintext.";
    let mut data_vec = origin_str.as_bytes().to_vec();

    let key_str = "BBBBBBBBBBBBBBBB";
    let nonce_str = "$$$$$$$$$$$$$$$$";

    let mut cipher = crypto::new_cipher(
        "AES-CTR",
        &format!("{{\"key\": \"{}\", \"nonce\": \"{}\"}}", key_str, nonce_str),
    )
    .unwrap();

    cipher.encrypt(&mut data_vec);

    // Recreate the cipher to decrypt.
    let mut cipher = crypto::new_cipher(
        "AES-CTR",
        &format!("{{\"key\": \"{}\", \"nonce\": \"{}\"}}", key_str, nonce_str),
    )
    .unwrap();

    cipher.encrypt(&mut data_vec);

    let data_str = String::from_utf8(data_vec).unwrap();
    println!("data: {:?}", data_str);

    assert_eq!(data_str, origin_str);
}

#[test]
fn test_aes_ctr_decrypt() {
    // Hexadecimal string of ciphertext.
    let hex_str = "37189CD37F16FB259496C99C03A93E2F";

    // Convert hexadecimal string to byte array.
    let mut decrypt_bytes = Vec::new();
    for i in 0..(hex_str.len() / 2) {
        let byte_str = &hex_str[i * 2..i * 2 + 2];
        let byte = u8::from_str_radix(byte_str, 16)
            .expect("Invalid hexadecimal string");
        decrypt_bytes.push(byte);
    }
    let mut data_vec = decrypt_bytes;

    let key_str = "0123456789012345";
    let nonce_str = "0123456789012345";

    let mut cipher = crypto::new_cipher(
        "AES-CTR",
        &format!("{{\"key\": \"{}\", \"nonce\": \"{}\"}}", key_str, nonce_str),
    )
    .unwrap();

    cipher.encrypt(&mut data_vec);

    // Convert decrypted bytes to string.
    let data_str = String::from_utf8(data_vec).unwrap_or_else(|e| {
        println!("Decrypted data is not a valid UTF-8 string: {}", e);
        String::new()
    });
    println!("Decrypted data: {}", data_str);
}

#[test]
fn test_aes_ctr_invalid_key() {
    let key_str = "00";
    let nonce_str = "0123456789012345";
    let cipher = crypto::new_cipher(
        "AES-CTR",
        &format!("{{\"key\": \"{}\", \"nonce\": \"{}\"}}", key_str, nonce_str),
    );

    assert!(cipher.is_err());
}

#[test]
fn test_aes_ctr_invalid_nonce() {
    let key_str = "0123456789012345";
    let nonce_str = "00";
    let cipher = crypto::new_cipher(
        "AES-CTR",
        &format!("{{\"key\": \"{}\", \"nonce\": \"{}\"}}", key_str, nonce_str),
    );

    assert!(cipher.is_err());
}
