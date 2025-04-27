//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::os::raw::c_char;

/// Mock implementation of ten_get_runtime_version for tests.
#[no_mangle]
pub extern "C" fn ten_get_runtime_version() -> *const c_char {
    "1.0.0".as_ptr() as *const c_char
}

/// Mock implementation of ten_get_global_log_path for tests.
#[no_mangle]
pub extern "C" fn ten_get_global_log_path() -> *const c_char {
    "/tmp/ten_runtime.log".as_ptr() as *const c_char
}
