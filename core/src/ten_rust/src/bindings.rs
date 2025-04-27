//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::ffi::{c_char, CString};

/// Frees a C string that was allocated by Rust.
///
/// # Safety
///
/// This function takes ownership of a raw pointer and frees it. The caller must
/// ensure that the pointer was originally allocated by Rust and that it is not
/// used after being freed. Passing a null pointer is safe, as the function will
/// simply return in that case.
#[no_mangle]
pub extern "C" fn ten_rust_free_cstring(ptr: *const c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        // Cast away const-ness to take ownership back and let it drop.
        let ptr = ptr as *mut c_char;
        let _ = CString::from_raw(ptr);
    }
}
