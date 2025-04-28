//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::ptr;
use std::{ffi::CStr, os::raw::c_char};

use crate::crypto;

use super::{Cipher, CipherAlgorithm};

/// Create a cipher.
///
/// # Parameter
/// - `algorithm`: The name of the encryption algorithm. e.g., "AES-CTR"
/// - `params`: The parameters of the encryption algorithm in JSON format.
///
/// # Return value
/// Returns a pointer to the cipher on success, otherwise returns `null`.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub extern "C" fn ten_cipher_create(
    algorithm: *const c_char,
    params: *const c_char,
) -> *mut Cipher {
    let algorithm_cstr = unsafe { CStr::from_ptr(algorithm) };
    let params_cstr = unsafe { CStr::from_ptr(params) };

    let algorithm_str = algorithm_cstr.to_str().unwrap_or_else(|err| {
        eprintln!("Error: {}", err);
        ""
    });
    if algorithm_str.is_empty() {
        return ptr::null_mut();
    }

    let params_str = params_cstr.to_str().unwrap_or_else(|err| {
        eprintln!("Error: {}", err);
        ""
    });
    if params_str.is_empty() {
        return ptr::null_mut();
    }

    let cipher = crypto::new_cipher(algorithm_str, params_str);

    match cipher {
        Ok(cipher) => Box::into_raw(Box::new(cipher)),
        Err(_) => ptr::null_mut(),
    }
}

/// Destroy a cipher.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub extern "C" fn ten_cipher_destroy(cipher_ptr: *mut Cipher) {
    if !cipher_ptr.is_null() {
        drop(unsafe { Box::from_raw(cipher_ptr) });
    }
}

/// Encrypt data in place.
///
/// # Parameter
/// - `cipher_ptr`: The pointer to the cipher.
/// - `data`: The pointer to the data to be encrypted.
/// - `data_len`: The length of the data to be encrypted.
///
/// # Return value
/// Returns `true` on success, otherwise returns `false`.
#[no_mangle]
pub unsafe extern "C" fn ten_cipher_encrypt_inplace(
    cipher_ptr: *mut Cipher,
    data: *mut u8,
    data_len: usize,
) -> bool {
    if cipher_ptr.is_null() || data.is_null() || data_len == 0 {
        return false;
    }

    let cipher = &mut *cipher_ptr;
    let data_slice = std::slice::from_raw_parts_mut(data, data_len);
    cipher.encrypt(data_slice);
    true
}
