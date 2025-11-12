//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::io;

use serde::{Deserialize, Serialize};
use tracing_subscriber::fmt::MakeWriter as TracingMakeWriter;

use crate::crypto::{new_cipher, Cipher, CipherAlgorithm};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AesCtrParams {
    pub key: String,
    pub nonce: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(untagged)]
pub enum EncryptionParams {
    AesCtr(AesCtrParams),
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct EncryptionConfig {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub enabled: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub algorithm: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub params: Option<EncryptionParams>,
}

#[derive(Debug, Clone)]
pub(super) struct EncryptionRuntimeConfig {
    pub(super) algorithm: String,
    pub(super) params_json: String,
}

impl EncryptionConfig {
    pub(super) fn to_runtime(&self) -> Option<EncryptionRuntimeConfig> {
        if !self.enabled.unwrap_or(false) {
            return None;
        }
        let alg = self.algorithm.as_ref()?;
        let params = self.params.as_ref()?;
        match (alg.as_str(), params) {
            ("AES-CTR", EncryptionParams::AesCtr(p)) => {
                let params_json = serde_json::to_string(p).ok()?;
                Some(EncryptionRuntimeConfig {
                    algorithm: alg.clone(),
                    params_json,
                })
            }
            _ => None,
        }
    }
}

pub(super) struct EncryptWriter<W: io::Write> {
    inner: W,
    cipher: Option<Cipher>,
}

impl<W: io::Write> EncryptWriter<W> {
    fn new(inner: W, cipher: Option<Cipher>) -> Self {
        Self {
            inner,
            cipher,
        }
    }

    fn write_encrypted_block(&mut self, payload: &[u8]) -> io::Result<()> {
        // If encryption is disabled, just write payload through
        let Some(cipher) = self.cipher.as_mut() else {
            self.inner.write_all(payload)?;
            return Ok(());
        };

        // Encrypt a copy of payload line-by-line
        let mut data = payload.to_vec();
        cipher.encrypt(&mut data);

        // Build 5-byte header following C implementation semantics
        // header[0] = 0xFF, header[1] = 0xFF
        // header[2] = version/reserved/parity bit (bit7), we use version 0
        // initially (0x00) header[3..4] = big-endian length of data
        // parity = XOR of the 5 header bytes before setting parity, then set
        // bit7 of header[2]
        let data_len = data.len();
        if data_len > u16::MAX as usize {
            // Should not happen for typical log lines; fallback to
            // write-through without header to avoid truncation
            self.inner.write_all(&data)?;
            return Ok(());
        }
        let mut header = [0u8; 5];
        header[0] = 0xFF;
        header[1] = 0xFF;
        header[2] = 0x00; // version 0, parity bit not set yet
        header[3] = ((data_len as u16) >> 8) as u8;
        header[4] = (data_len as u16 & 0xFF) as u8;

        let mut parity: u8 = 0;
        for b in header {
            parity ^= b;
        }
        // Set bit7 (MSB) of header[2] to parity & 1
        header[2] |= (parity & 0x01) << 7;

        // Write header + ciphertext
        self.inner.write_all(&header)?;
        self.inner.write_all(&data)?;
        Ok(())
    }
}

impl<W: io::Write> io::Write for EncryptWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if self.cipher.is_none() {
            return self.inner.write(buf);
        }
        self.write_encrypted_block(buf)?;
        Ok(buf.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

#[derive(Clone)]
pub(super) struct EncryptMakeWriter<M> {
    pub(super) inner: M,
    pub(super) runtime: EncryptionRuntimeConfig,
}

impl<'writer, M> TracingMakeWriter<'writer> for EncryptMakeWriter<M>
where
    M: TracingMakeWriter<'writer> + Clone,
{
    type Writer = EncryptWriter<M::Writer>;

    fn make_writer(&'writer self) -> Self::Writer {
        let inner = self.inner.make_writer();
        let cipher = new_cipher(&self.runtime.algorithm, &self.runtime.params_json).ok();
        EncryptWriter::new(inner, cipher)
    }
}
