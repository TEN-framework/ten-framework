//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Mutex;

use aes::cipher::StreamCipherSeek;
use aes::Aes128;
use anyhow::Result;
use ctr::cipher::{KeyIvInit, StreamCipher};
use serde::de::{self};
use serde::{Deserialize, Deserializer};

use crate::crypto::CipherAlgorithm;

type Aes128Ctr128BE = ctr::Ctr128BE<Aes128>;

pub struct AesCtrConfig {
    pub key: [u8; 16],
    pub nonce: [u8; 16],
}

pub struct AesCtrCipher {
    cipher: Mutex<Aes128Ctr128BE>,
}

pub fn new_aes_ctr_cipher(params: &str) -> Result<AesCtrCipher> {
    let config: AesCtrConfig = serde_json::from_str(params)?;
    let cipher = Aes128Ctr128BE::new(&config.key.into(), &config.nonce.into());
    Ok(AesCtrCipher {
        cipher: Mutex::new(cipher),
    })
}

impl<'de> Deserialize<'de> for AesCtrConfig {
    fn deserialize<D>(deserializer: D) -> std::result::Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Deserialize)]
        struct RawParams {
            key: String,
            nonce: String,
        }

        let raw = RawParams::deserialize(deserializer)?;

        let key: [u8; 16] = raw
            .key
            .as_bytes()
            .try_into()
            .map_err(|_| de::Error::custom("key must be 16 bytes"))?;

        let nonce: [u8; 16] = raw
            .nonce
            .as_bytes()
            .try_into()
            .map_err(|_| de::Error::custom("nonce must be 16 bytes"))?;

        Ok(AesCtrConfig { key, nonce })
    }
}

impl CipherAlgorithm for AesCtrCipher {
    fn encrypt(&mut self, data: &mut [u8]) {
        let mut cipher = self.cipher.lock().unwrap();
        cipher.seek(0u32);
        cipher.apply_keystream(data);
    }
}
