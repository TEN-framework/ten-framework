//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::{anyhow, Result};

use crate::crypto::{new_cipher, Cipher, CipherAlgorithm};

/// Decrypt records from a byte slice produced by the encrypted log writer.
///
/// The format is a sequence of records:
/// - 5-byte header: [0]=0xFF, [1]=0xFF, [2]=version+parity(bit7), [3..4]=len
///   (big-endian)
/// - followed by `len` bytes of ciphertext
///
/// This function validates magic and parity, decrypts each record, and
/// concatenates all plaintexts in order.
pub fn decrypt_records_bytes(algorithm: &str, params_json: &str, input: &[u8]) -> Result<Vec<u8>> {
    let mut cipher: Cipher = new_cipher(algorithm, params_json)?;
    let mut pos: usize = 0;
    let mut out: Vec<u8> = Vec::new();

    while pos + 5 <= input.len() {
        let header = &input[pos..pos + 5];
        if header[0] != 0xFF || header[1] != 0xFF {
            return Err(anyhow!("invalid magic header at pos {pos}"));
        }
        let parity_bit = (header[2] >> 7) & 1;
        let calc = (header[0] ^ header[1] ^ (header[2] & 0x7F) ^ header[3] ^ header[4]) & 1;
        if parity_bit != calc {
            return Err(anyhow!("parity mismatch at pos {pos}"));
        }

        let len = ((header[3] as usize) << 8) | (header[4] as usize);
        if pos + 5 + len > input.len() {
            return Err(anyhow!("record length out of range at pos {pos}"));
        }

        let mut block = input[pos + 5..pos + 5 + len].to_vec();
        // Decrypt in-place (stream cipher, keystream restarted in our encrypt
        // API)
        cipher.encrypt(&mut block);
        out.extend_from_slice(&block);
        pos += 5 + len;
    }

    if pos != input.len() {
        return Err(anyhow!("trailing bytes without complete record"));
    }

    Ok(out)
}
