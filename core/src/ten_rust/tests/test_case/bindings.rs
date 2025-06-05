//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::ffi::{CStr, CString};
    use ten_rust::bindings::{
        ten_rust_free_cstring, ten_rust_graph_validate_complete_flatten,
    };

    #[test]
    fn test_ten_rust_graph_from_str_valid_json() {
        let input_json = r#"{
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension",
                    "addon": "test_addon"
                }
            ]
        }"#;

        let c_input = CString::new(input_json).unwrap();
        let result_ptr = unsafe {
            ten_rust_graph_validate_complete_flatten(c_input.as_ptr())
        };

        assert!(!result_ptr.is_null());

        // Convert the result back to a Rust string to verify it's valid JSON
        let result_cstr = unsafe { CStr::from_ptr(result_ptr) };
        let result_str = result_cstr.to_str().unwrap();

        // Parse the result to ensure it's valid JSON
        let parsed: serde_json::Value =
            serde_json::from_str(result_str).unwrap();
        assert!(parsed.is_object());
        assert!(parsed["nodes"].is_array());

        // Free the allocated string
        ten_rust_free_cstring(result_ptr);
    }

    #[test]
    fn test_ten_rust_graph_from_str_invalid_json() {
        let input_json = "invalid json";

        let c_input = CString::new(input_json).unwrap();
        let result_ptr = unsafe {
            ten_rust_graph_validate_complete_flatten(c_input.as_ptr())
        };

        // Should return null for invalid JSON
        assert!(result_ptr.is_null());
    }

    #[test]
    fn test_ten_rust_graph_from_str_null_input() {
        let result_ptr = unsafe {
            ten_rust_graph_validate_complete_flatten(std::ptr::null())
        };

        // Should return null for null input
        assert!(result_ptr.is_null());
    }

    #[test]
    fn test_ten_rust_free_cstring_null_input() {
        // Should not crash with null input
        ten_rust_free_cstring(std::ptr::null());
    }

    #[test]
    fn test_ten_rust_graph_from_str_empty_graph() {
        let input_json = r#"{
            "nodes": []
        }"#;

        let c_input = CString::new(input_json).unwrap();
        let result_ptr = unsafe {
            ten_rust_graph_validate_complete_flatten(c_input.as_ptr())
        };

        assert!(!result_ptr.is_null());

        // Convert the result back to a Rust string
        let result_cstr = unsafe { CStr::from_ptr(result_ptr) };
        let result_str = result_cstr.to_str().unwrap();

        // Parse the result to ensure it's valid JSON
        let parsed: serde_json::Value =
            serde_json::from_str(result_str).unwrap();
        assert!(parsed.is_object());
        assert!(parsed["nodes"].is_array());
        assert_eq!(parsed["nodes"].as_array().unwrap().len(), 0);

        // Free the allocated string
        ten_rust_free_cstring(result_ptr);
    }
}
