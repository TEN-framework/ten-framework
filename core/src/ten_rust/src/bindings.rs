//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::ffi::{c_char, CStr, CString};

use crate::graph::graph_info::GraphInfo;

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

/// Parses a JSON string into a GraphInfo and returns it as a JSON string.
///
/// This function takes a C string containing JSON, parses it into a GraphInfo
/// structure, validates and processes it, then serializes it back to JSON.
///
/// # Parameters
/// - `json_str`: A null-terminated C string containing the JSON representation
///   of a graph. Must not be NULL.
/// - `current_base_dir`: A null-terminated C string containing the current base
///   directory. Can be NULL if the current base directory is not known.
/// - `err_msg`: Pointer to a char* that will be set to an error message if the
///   function fails. Can be NULL if error details are not needed. If set, the
///   error message must be freed using `ten_rust_free_cstring()`.
///
/// # Returns
/// - On success: A pointer to a newly allocated C string containing the
///   processed graph JSON
/// - On failure: NULL pointer
///
/// # Safety
///
/// The caller must ensure that:
/// - `json_str` is a valid null-terminated C string
/// - `current_base_dir` is a valid null-terminated C string, or NULL
/// - The returned pointer (if not null) is freed using
///   `ten_rust_free_cstring()`
/// - If err_msg is not NULL, the error message (if set) must be freed using
///   `ten_rust_free_cstring()`
/// - The input string contains valid UTF-8 encoded JSON
///
/// # Memory Management
///
/// Both the returned string and error message (if set) are allocated by Rust
/// and must be freed by calling `ten_rust_free_cstring()` when no longer
/// needed.
///
/// # Example
/// ```c
/// const char* input_json = "{\"nodes\": []}";
/// const char* current_base_dir = "/path/to/current/base/dir";
/// char* err_msg = NULL;
/// const char* result =
///     ten_rust_predefined_graph_validate_complete_flatten(
///         input_json, current_base_dir, &err_msg);
/// if (result != NULL) {
///     printf("Processed graph: %s\n", result);
///     ten_rust_free_cstring(result);
/// } else if (err_msg != NULL) {
///     printf("Failed to process graph: %s\n", err_msg);
///     ten_rust_free_cstring(err_msg);
/// } else {
///     printf("Failed to process graph\n");
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn ten_rust_predefined_graph_validate_complete_flatten(
    json_str: *const c_char,
    current_base_dir: *const c_char,
    err_msg: *mut *mut c_char,
) -> *const c_char {
    if json_str.is_null() {
        if !err_msg.is_null() {
            let err_msg_c_str = CString::new("json_str is null").unwrap();
            *err_msg = err_msg_c_str.into_raw();
        }
        return std::ptr::null();
    }

    // Convert C string to Rust string
    let json_str_c_str = CStr::from_ptr(json_str);
    let json_str_rust_str = match json_str_c_str.to_str() {
        Ok(s) => s,
        Err(e) => {
            if !err_msg.is_null() {
                let err_msg_c_str = CString::new(e.to_string()).unwrap();
                *err_msg = err_msg_c_str.into_raw();
            }
            return std::ptr::null(); // Invalid UTF-8
        }
    };

    let current_base_dir_rust_str = if current_base_dir.is_null() {
        None
    } else {
        let current_base_dir_c_str = CStr::from_ptr(current_base_dir);
        let current_base_dir_rust_str = match current_base_dir_c_str.to_str() {
            Ok(s) => s,
            Err(e) => {
                if !err_msg.is_null() {
                    let err_msg_c_str = CString::new(e.to_string()).unwrap();
                    *err_msg = err_msg_c_str.into_raw();
                }
                return std::ptr::null(); // Invalid UTF-8
            }
        };
        Some(current_base_dir_rust_str)
    };

    // Parse the JSON string into a Graph
    let graph_info = match GraphInfo::from_str_with_base_dir(
        json_str_rust_str,
        current_base_dir_rust_str,
    ) {
        Ok(g) => g,
        Err(e) => {
            if !err_msg.is_null() {
                let err_msg_c_str = CString::new(e.to_string()).unwrap();
                *err_msg = err_msg_c_str.into_raw();
            }
            return std::ptr::null(); // Parsing failed
        }
    };

    // Serialize the graph back to JSON
    let json_output = match serde_json::to_string(&graph_info) {
        Ok(json) => json,
        Err(e) => {
            if !err_msg.is_null() {
                let err_msg_c_str = CString::new(e.to_string()).unwrap();
                *err_msg = err_msg_c_str.into_raw();
            }
            return std::ptr::null(); // Serialization failed
        }
    };

    // Convert to C string
    match CString::new(json_output) {
        Ok(c_string) => c_string.into_raw(),
        Err(e) => {
            if !err_msg.is_null() {
                let err_msg_c_str = CString::new(e.to_string()).unwrap();
                *err_msg = err_msg_c_str.into_raw();
            }
            std::ptr::null() // Contains null bytes
        }
    }
}
