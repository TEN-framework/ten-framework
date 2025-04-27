//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    ffi::c_char,
    sync::{Arc, Mutex},
};

use anyhow::{Ok, Result};

use crate::pkg_info::value_type::ValueType;

use super::bindings::{
    ten_schema_adjust_and_validate_json_str_proxy,
    ten_schema_create_from_json_str_proxy, ten_schema_destroy_proxy,
    ten_schema_is_compatible_proxy, ten_schema_t,
};

pub struct TenSchemaOfField {
    pub field_type: ValueType,
}

/// `TenSchemaRawPointer` acts as a proxy for the raw pointer to `ten_schema_t`
/// in C. When the Rust object bound to this pointer is dropped, we need to
/// ensure that the raw pointer is properly destroyed. However, the `Drop` trait
/// cannot be directly implemented on a raw pointer.
///
/// This wrapper provides safe memory management for the C-allocated schema
/// object, ensuring proper cleanup when the Rust object goes out of scope.
#[derive(Debug)]
struct TenSchemaRawPointer {
    /// The raw pointer to the C-allocated schema object.
    /// Protected by a Mutex to allow safe sharing between threads.
    raw_ptr: Mutex<*const ten_schema_t>,
}

// Explicitly mark this type as thread-safe since we're handling raw pointers.
// While raw pointers are not automatically Send or Sync in Rust, we can
// safely implement these traits because:
//
// 1. The raw_ptr is protected by a Mutex, ensuring exclusive access during
//    operations.
// 2. The pointer is effectively read-only after initialization.
// 3. The actual mutation only happens during Drop, which has exclusive access.
//
// This allows TenSchema to be safely shared between threads in the application.
unsafe impl Send for TenSchemaRawPointer {}
unsafe impl Sync for TenSchemaRawPointer {}

impl Drop for TenSchemaRawPointer {
    fn drop(&mut self) {
        // Acquire lock on the pointer to ensure exclusive access during
        // cleanup.
        let ptr = self.raw_ptr.lock().unwrap();

        // Only attempt to free non-null pointers to avoid undefined behavior.
        if !ptr.is_null() {
            unsafe {
                // Call the C function to properly free the schema resources.
                ten_schema_destroy_proxy(*ptr);
            }
        }
    }
}

#[derive(Debug, Clone)]
pub struct TenSchema {
    // TenSchema represents a schema definition that validates TEN values. It
    // needs to implement Clone for use in various contexts, and must be
    // thread-safe for use in the tman designer where it will be shared between
    // threads. We use Arc to wrap the raw pointer to ensure safe concurrent
    // access and proper reference counting.
    inner: Arc<TenSchemaRawPointer>,
}

impl TenSchema {
    /// Creates a new TenSchema instance from a raw C pointer to a ten_schema_t.
    ///
    /// This constructor takes ownership of the provided raw pointer and ensures
    /// it will be properly cleaned up when the TenSchema is dropped.
    ///
    /// # Safety
    /// The caller must ensure that:
    /// - The raw_ptr is a valid pointer to a ten_schema_t structure.
    /// - The raw_ptr was created by the C API
    ///   (ten_schema_create_from_json_str_proxy).
    /// - Ownership of the pointer is transferred to this TenSchema instance.
    pub fn new(raw_ptr: *const ten_schema_t) -> Self {
        Self {
            // The raw_ptr is wrapped in a Mutex and Arc to ensure thread-safety
            // and proper reference counting. While the raw_ptr itself is not
            // inherently Send or Sync, our wrapper provides safe concurrent
            // access.
            inner: Arc::new(TenSchemaRawPointer {
                raw_ptr: Mutex::new(raw_ptr),
            }),
        }
    }

    pub fn as_ptr(&self) -> *const ten_schema_t {
        *self.inner.raw_ptr.lock().unwrap()
    }

    /// Validates a JSON value against this schema.
    ///
    /// This method checks if the provided JSON value conforms to the schema.
    pub fn validate_json(&self, value: &serde_json::Value) -> Result<()> {
        // Convert the JSON value to a string for passing to C.
        let value_str = serde_json::to_string(value)?;
        let mut err_msg: *mut c_char = std::ptr::null_mut();

        unsafe {
            // Create a C-compatible string from the JSON string.
            let c_value_str = std::ffi::CString::new(value_str)?;

            // Call the C function to validate the string against the schema.
            let success = ten_schema_adjust_and_validate_json_str_proxy(
                self.as_ptr(),
                c_value_str.as_ptr(),
                &mut err_msg as *mut _ as *mut *const c_char,
            );

            if !success {
                // If validation failed, convert the C error message to a Rust
                // string.
                let rust_err_msg =
                    std::ffi::CStr::from_ptr(err_msg).to_str()?.to_owned();
                // The err_msg is allocated by the C code, so we need to free
                // it to avoid memory leaks.
                libc::free(err_msg as *mut libc::c_void);

                return Err(anyhow::anyhow!(rust_err_msg));
            }
        }

        Ok(())
    }

    /// Checks if this schema is compatible with the target schema.
    ///
    /// This method determines if data conforming to this schema can be safely
    /// processed by a component expecting data conforming to the target schema.
    /// Compatibility is directional - if A is compatible with B, it doesn't
    /// mean B is compatible with A.
    pub fn is_compatible_with(&self, target: &TenSchema) -> Result<()> {
        let mut err_msg: *mut c_char = std::ptr::null_mut();
        unsafe {
            // Call the C function to check schema compatibility.
            let success = ten_schema_is_compatible_proxy(
                self.as_ptr(),
                target.as_ptr(),
                &mut err_msg as *mut _ as *mut *const c_char,
            );

            if !success {
                // If compatibility check failed, convert the C error message to
                // a Rust string
                let rust_err_msg =
                    std::ffi::CStr::from_ptr(err_msg).to_str()?.to_owned();
                // The err_msg is allocated by the C code, so we need to free
                // it to avoid memory leaks.
                libc::free(err_msg as *mut libc::c_void);

                return Err(anyhow::anyhow!(rust_err_msg));
            }
        }

        Ok(())
    }
}

pub fn create_schema_from_json(
    schema_json: &serde_json::Value,
) -> Result<TenSchema> {
    let schema_str = serde_json::to_string(schema_json)?;
    create_schema_from_json_str(&schema_str)
}

pub fn create_schema_from_json_str(schema_json_str: &str) -> Result<TenSchema> {
    let mut err_msg: *mut c_char = std::ptr::null_mut();
    let schema_ptr: *mut ten_schema_t;
    unsafe {
        let c_schema_str = std::ffi::CString::new(schema_json_str)?;
        schema_ptr = ten_schema_create_from_json_str_proxy(
            c_schema_str.as_ptr(),
            &mut err_msg as *mut _ as *mut *const c_char,
        );
    }

    if schema_ptr.is_null() {
        // Failed to create ten_schema_t in C world.
        unsafe {
            let rust_err_msg =
                std::ffi::CStr::from_ptr(err_msg).to_str()?.to_owned();
            // The err_msg is allocated by the C code, so we need to free it.
            libc::free(err_msg as *mut libc::c_void);

            return Err(anyhow::anyhow!(rust_err_msg));
        }
    }

    Ok(TenSchema::new(schema_ptr))
}
