//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::ffi::{c_char, CStr, CString};

use crate::graph::Graph;

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

/// Parses a JSON string into a Graph and returns it as a JSON string.
///
/// This function wraps the Graph::from_str_with_base_dir() functionality for C
/// code. It takes a C string containing JSON, parses it into a Graph structure,
/// validates and processes it, then serializes it back to JSON.
///
/// # Parameters
/// - `json_str`: A null-terminated C string containing the JSON representation
///   of a graph
///
/// # Returns
/// - On success: A pointer to a newly allocated C string containing the
///   processed graph JSON
/// - On failure: A null pointer
///
/// # Safety
///
/// The caller must ensure that:
/// - `json_str` is a valid null-terminated C string
/// - The returned pointer (if not null) is freed using
///   `ten_rust_free_cstring()`
/// - The input string contains valid UTF-8 encoded JSON
///
/// # Memory Management
///
/// The returned string is allocated by Rust and must be freed by calling
/// `ten_rust_free_cstring()` when no longer needed.
#[no_mangle]
pub unsafe extern "C" fn ten_rust_graph_validate_complete_flatten(
    json_str: *const c_char,
) -> *const c_char {
    if json_str.is_null() {
        return std::ptr::null();
    }

    // Convert C string to Rust string
    let c_str = CStr::from_ptr(json_str);

    let rust_str = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null(), // Invalid UTF-8
    };

    // Parse the JSON string into a Graph
    let graph = match Graph::from_str_with_base_dir(rust_str, None) {
        Ok(g) => g,
        Err(_) => return std::ptr::null(), // Parsing failed
    };

    // Serialize the graph back to JSON
    let json_output = match serde_json::to_string(&graph) {
        Ok(json) => json,
        Err(_) => return std::ptr::null(), // Serialization failed
    };

    // Convert to C string
    match CString::new(json_output) {
        Ok(c_string) => c_string.into_raw(),
        Err(_) => std::ptr::null(), // Contains null bytes
    }
}
