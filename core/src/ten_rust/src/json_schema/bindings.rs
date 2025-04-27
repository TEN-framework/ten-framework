//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::ffi::{c_char, CStr, CString};

use crate::json_schema;

/// Validates the manifest json string.
#[no_mangle]
pub extern "C" fn ten_validate_manifest_json_string(
    manifest_data: *const c_char,
    out_err_msg: *mut *const c_char,
) -> bool {
    assert!(!manifest_data.is_null(), "Invalid argument.");
    let c_manifest_data = unsafe { CStr::from_ptr(manifest_data) };

    // Note that no copy is made here. You can call 'rust_path.to_owned()' if
    // you need a copy.
    let rust_manifest_data = c_manifest_data.to_str().unwrap();
    let ret =
        json_schema::ten_validate_manifest_json_string(rust_manifest_data);
    if ret.is_err() {
        let err_msg = ret.err().unwrap().to_string();
        let c_err_msg =
            CString::new(err_msg).expect("Failed to allocate memory.");
        unsafe {
            *out_err_msg = c_err_msg.into_raw();
        }
        return false;
    }
    true
}

/// Validates the manifest json file.
#[no_mangle]
pub extern "C" fn ten_validate_manifest_json_file(
    manifest_file: *const c_char,
    out_err_msg: *mut *const c_char,
) -> bool {
    assert!(!manifest_file.is_null(), "Invalid argument.");
    let c_manifest_file = unsafe { CStr::from_ptr(manifest_file) };

    // Note that no copy is made here. You can call 'rust_path.to_owned()' if
    // you need a copy.
    let rust_manifest_file = c_manifest_file.to_str().unwrap();
    let ret = json_schema::ten_validate_manifest_json_file(rust_manifest_file);
    if ret.is_err() {
        let err_msg = ret.err().unwrap().to_string();
        let c_err_msg =
            CString::new(err_msg).expect("Failed to allocate memory.");
        unsafe {
            *out_err_msg = c_err_msg.into_raw();
        }
        return false;
    }
    true
}

/// Validates the property json string.
#[no_mangle]
pub extern "C" fn ten_validate_property_json_string(
    property_data: *const c_char,
    out_err_msg: *mut *const c_char,
) -> bool {
    assert!(!property_data.is_null(), "Invalid argument.");
    let c_property_data = unsafe { CStr::from_ptr(property_data) };

    // Note that no copy is made here. You can call 'rust_path.to_owned()' if
    // you need a copy.
    let rust_property_data = c_property_data.to_str().unwrap();
    let ret =
        json_schema::ten_validate_property_json_string(rust_property_data);
    if ret.is_err() {
        let err_msg = ret.err().unwrap().to_string();
        let c_err_msg =
            CString::new(err_msg).expect("Failed to allocate memory.");
        unsafe {
            *out_err_msg = c_err_msg.into_raw();
        }
        return false;
    }
    true
}

/// Validates the property json file.
#[no_mangle]
pub extern "C" fn ten_validate_property_json_file(
    property_file: *const c_char,
    out_err_msg: *mut *const c_char,
) -> bool {
    assert!(!property_file.is_null(), "Invalid argument.");
    let c_property_file = unsafe { CStr::from_ptr(property_file) };

    // Note that no copy is made here. You can call 'rust_path.to_owned()' if
    // you need a copy.
    let rust_property_file = c_property_file.to_str().unwrap();
    let ret = json_schema::ten_validate_property_json_file(rust_property_file);
    if ret.is_err() {
        let err_msg = ret.err().unwrap().to_string();
        let c_err_msg =
            CString::new(err_msg).expect("Failed to allocate memory.");
        unsafe {
            *out_err_msg = c_err_msg.into_raw();
        }
        return false;
    }
    true
}
