//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
//! FFI bindings for TEN logging system.
//!
//! This module provides Foreign Function Interface (FFI) bindings between C and
//! Rust for the TEN Framework's logging functionality.
//!
//! # Critical Safety Requirements
//!
//! ## Structure Layout Synchronization
//!
//! The [`TenLogLocInfo`] struct in this module MUST be kept in sync with the C
//! struct `ten_log_loc_info_t` defined in `core/include/ten_utils/log/log.h`.
//!
//! ### Verification Methods
//!
//! 1. **Compile-time checks**: The code includes `const` assertions that
//!    verify:
//!    - Total struct size matches expected layout
//!    - Struct alignment matches pointer alignment
//!    - These checks will cause compilation to FAIL if layout is incorrect
//!
//! 2. **Runtime tests**: Run the following command to verify memory layout:
//!    ```bash cargo test test_ten_log_loc_info_layout -- --nocapture ``` This
//!    test verifies:
//!    - Total struct size (48 bytes on 64-bit, 24 bytes on 32-bit)
//!    - Struct alignment (8 bytes on 64-bit, 4 bytes on 32-bit)
//!    - Individual field offsets match expected C layout
//!
//!    Note: The actual test implementation is in
//!    `tests/test_case/log/mod.rs::test_ten_log_loc_info_layout`
//!
//! ### Maintenance Workflow
//!
//! **When modifying `TenLogLocInfo` (Rust side):**
//! 1. Update the C definition in `core/include/ten_utils/log/log.h`
//! 2. Run `cargo test test_ten_log_loc_info_layout` to verify
//! 3. Run full C/C++ compilation to ensure compatibility
//!
//! **When modifying `ten_log_loc_info_t` (C side):**
//! 1. Update the Rust definition in this file
//! 2. Run `cargo test test_ten_log_loc_info_layout` to verify
//! 3. Run full C/C++ compilation to ensure compatibility
//!
//! ### What Happens If They Diverge?
//!
//! - **At compile time**: Const assertions will fail, preventing compilation
//! - **At runtime**: Undefined behavior, memory corruption, crashes
//! - **In tests**: Layout verification test will fail with detailed diagnostics
use std::{
    ffi::{CStr, CString},
    os::raw::c_char,
};

use crate::log::{ten_configure_log, ten_log_reopen_all, AdvancedLogConfig};

/// FFI-safe structure to pass location information from C to Rust.
///
/// # CRITICAL: Keep in sync with C definition
///
/// This struct MUST exactly match the C struct `ten_log_loc_info_t` defined in:
/// `core/include/ten_utils/log/log.h`
///
/// ## C Definition (for reference):
/// ```c
/// typedef struct ten_log_loc_info_t {
///   const char *app_uri;
///   size_t app_uri_len;
///   const char *graph_id;
///   size_t graph_id_len;
///   const char *extension_name;
///   size_t extension_name_len;
/// } ten_log_loc_info_t;
/// ```
///
/// ## Memory Layout Requirements:
/// - Field order must match exactly
/// - Field types must have same size and alignment as C counterparts
/// - `#[repr(C)]` ensures C-compatible memory layout
///
/// ## Safety:
/// - Modifying this struct requires updating the C definition
/// - Modifying the C definition requires updating this struct
/// - Static assertions below verify size and alignment at compile time
///
/// ## Verification:
/// Run `cargo test test_ten_log_loc_info_layout` to verify layout
/// compatibility.
#[repr(C)]
pub struct TenLogLocInfo {
    pub app_uri: *const c_char,
    pub app_uri_len: usize,
    pub graph_id: *const c_char,
    pub graph_id_len: usize,
    pub extension_name: *const c_char,
    pub extension_name_len: usize,
}

// Compile-time assertions to ensure struct layout matches C definition.
// These will cause compilation to fail if the struct layout changes
// unexpectedly.
#[allow(clippy::no_effect)]
const _: () = {
    // Expected size: 6 fields * pointer/usize size
    // On 64-bit: 6 * 8 = 48 bytes
    // On 32-bit: 6 * 4 = 24 bytes
    const EXPECTED_SIZE: usize = core::mem::size_of::<*const c_char>() * 6;
    const ACTUAL_SIZE: usize = core::mem::size_of::<TenLogLocInfo>();

    // This will cause a compile error if sizes don't match
    const SIZE_MATCHES: bool = EXPECTED_SIZE == ACTUAL_SIZE;
    [(); 1][!SIZE_MATCHES as usize];

    // Verify alignment matches pointer alignment
    const EXPECTED_ALIGN: usize = core::mem::align_of::<*const c_char>();
    const ACTUAL_ALIGN: usize = core::mem::align_of::<TenLogLocInfo>();

    const ALIGN_MATCHES: bool = EXPECTED_ALIGN == ACTUAL_ALIGN;
    [(); 1][!ALIGN_MATCHES as usize];
};

impl TenLogLocInfo {
    /// Helper to safely extract strings from the C struct.
    pub fn to_strings(&self) -> (&str, &str, &str) {
        let app_uri = if self.app_uri_len > 0 && !self.app_uri.is_null() {
            unsafe { CStr::from_ptr(self.app_uri).to_str().unwrap_or("") }
        } else {
            ""
        };

        let graph_id = if self.graph_id_len > 0 && !self.graph_id.is_null() {
            unsafe { CStr::from_ptr(self.graph_id).to_str().unwrap_or("") }
        } else {
            ""
        };

        let extension_name = if self.extension_name_len > 0 && !self.extension_name.is_null() {
            unsafe { CStr::from_ptr(self.extension_name).to_str().unwrap_or("") }
        } else {
            ""
        };

        (app_uri, graph_id, extension_name)
    }
}

/// Configure the log.
///
/// # Parameter
/// - `log_config_json`: The log configuration in JSON format.
///
/// # Return value
/// Returns a pointer to the log configuration on success, otherwise returns
/// `null`.
///
/// # Errors
/// Returns `null` if the log configuration is invalid.
///
/// # Examples
/// ```
/// ```
///
/// # Safety
/// The caller must free the returned pointer using `free` after use.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_rust_create_log_config_from_json(
    log_config_json: *const c_char,
    err_msg: *mut *mut c_char,
) -> *const AdvancedLogConfig {
    if log_config_json.is_null() {
        if !err_msg.is_null() {
            let err_msg_c_str = CString::new("Log config is null").unwrap();
            *err_msg = err_msg_c_str.into_raw();
        }
        return std::ptr::null();
    }

    let log_config_json = unsafe { CStr::from_ptr(log_config_json) };
    let log_config_json_str = match log_config_json.to_str() {
        Ok(log_config_json_str) => log_config_json_str,
        Err(e) => {
            if !err_msg.is_null() {
                let err_msg_c_str =
                    CString::new(format!("Failed to convert log config to JSON: {e:?}")).unwrap();
                *err_msg = err_msg_c_str.into_raw();
            }
            return std::ptr::null();
        }
    };

    let log_config: AdvancedLogConfig = match serde_json::from_str(log_config_json_str) {
        Ok(log_config) => log_config,
        Err(e) => {
            if !err_msg.is_null() {
                let err_msg_c_str =
                    CString::new(format!("Failed to parse log config: {e:?}")).unwrap();
                *err_msg = err_msg_c_str.into_raw();
            }
            return std::ptr::null();
        }
    };

    Box::into_raw(Box::new(log_config))
}

/// Configure the log.
///
/// # Parameter
/// - `config`: The log configuration.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub extern "C" fn ten_rust_configure_log(
    config: *mut AdvancedLogConfig,
    reloadable: bool,
    err_msg: *mut *mut c_char,
) -> bool {
    if config.is_null() {
        if !err_msg.is_null() {
            let err_msg_c_str = CString::new("Log config is null").unwrap();
            unsafe {
                *err_msg = err_msg_c_str.into_raw();
            }
        }
        return false;
    }

    let config = unsafe { &mut *config };

    let mut result = true;

    ten_configure_log(config, reloadable).unwrap_or_else(|e| {
        if !err_msg.is_null() {
            let err_msg_c_str = CString::new(e.to_string()).unwrap();
            unsafe {
                *err_msg = err_msg_c_str.into_raw();
            }
        }
        result = false;
    });

    result
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub extern "C" fn ten_rust_log_reopen_all(
    config: *mut AdvancedLogConfig,
    reloadable: bool,
    err_msg: *mut *mut c_char,
) -> bool {
    if config.is_null() {
        if !err_msg.is_null() {
            let err_msg_c_str = CString::new("Log config is null").unwrap();
            unsafe {
                *err_msg = err_msg_c_str.into_raw();
            }
        }
        return false;
    }

    let config = unsafe { &mut *config };

    ten_log_reopen_all(config, reloadable);

    true
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub extern "C" fn ten_rust_log(
    config: *const AdvancedLogConfig,
    category: *const c_char,
    category_len: usize,
    pid: i64,
    tid: i64,
    level: i32,
    func_name: *const c_char,
    func_name_len: usize,
    file_name: *const c_char,
    file_name_len: usize,
    line_no: u32,
    loc_info: *const TenLogLocInfo,
    msg: *const c_char,
    msg_len: usize,
) {
    if config.is_null()
        || func_name_len == 0
        || file_name_len == 0
        || msg_len == 0
        || func_name.is_null()
        || file_name.is_null()
        || msg.is_null()
    {
        return;
    }

    let config = unsafe { &*config };

    let log_level = crate::log::LogLevel::from(level as u8);

    // Parse location info.
    let (app_uri, graph_id, extension_name) =
        if !loc_info.is_null() { unsafe { (*loc_info).to_strings() } } else { ("", "", "") };

    let func_name_str = match unsafe { CStr::from_ptr(func_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return,
    };

    let file_name_str = match unsafe { CStr::from_ptr(file_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return,
    };

    let msg_str = match unsafe { CStr::from_ptr(msg) }.to_str() {
        Ok(s) => s,
        Err(_) => return,
    };

    let category_str = if category_len == 0 || category.is_null() {
        ""
    } else {
        match unsafe { CStr::from_ptr(category) }.to_str() {
            Ok(s) => s,
            Err(_) => return,
        }
    };

    crate::log::ten_log(
        config,
        category_str,
        pid,
        tid,
        log_level,
        func_name_str,
        file_name_str,
        line_no,
        app_uri,
        graph_id,
        extension_name,
        msg_str,
    );
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub extern "C" fn ten_rust_log_config_destroy(config: *mut AdvancedLogConfig) {
    if !config.is_null() {
        drop(unsafe { Box::from_raw(config) });
    }
}
