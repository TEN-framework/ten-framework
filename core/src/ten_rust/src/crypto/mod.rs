//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod algorithm;
mod bindings;

use anyhow::Result;

use crate::crypto::algorithm::aes::new_aes_ctr_cipher;
use crate::crypto::algorithm::aes::AesCtrCipher;

pub enum Cipher {
    Aes128Ctr(AesCtrCipher),
}

pub trait CipherAlgorithm {
    fn encrypt(&mut self, data: &mut [u8]);
}

impl CipherAlgorithm for Cipher {
    fn encrypt(&mut self, data: &mut [u8]) {
        match self {
            Cipher::Aes128Ctr(cipher) => {
                cipher.encrypt(data);
            }
        }
    }
}

pub fn new_cipher(algorithm: &str, params_str: &str) -> Result<Cipher> {
    match algorithm {
        "AES-CTR" => {
            let cipher = new_aes_ctr_cipher(params_str)?;
            Ok(Cipher::Aes128Ctr(cipher))
        }
        _ => Err(anyhow::anyhow!("Invalid encryption algorithm")),
    }
}
