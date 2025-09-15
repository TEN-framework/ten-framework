//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use core::ffi::{c_char, c_int, c_void};
use std::{ffi::CString, ptr};

use backtrace;

/// This function is a wrapper for the backtrace::trace and backtrace::resolve
/// functions. It is used to dump the backtrace of the current function.
/// It is called by the C function ten_rust_backtrace_dump.
#[no_mangle]
pub extern "C" fn ten_rust_backtrace_dump(
    ctx: *mut c_void,
    on_dump: Option<
        extern "C" fn(
            ctx: *mut c_void,
            pc: usize,
            filename: *const c_char,
            lineno_c: c_int,
            function: *const c_char,
            data: *mut c_void,
        ) -> c_int,
    >,
    on_error: Option<
        extern "C" fn(ctx: *mut c_void, msg: *const c_char, errnum: c_int, data: *mut c_void),
    >,
    skip: usize,
) -> c_int {
    // on_dump is a required parameter: without it, we cannot call the callback for
    // each frame to the C side
    let on_dump_cb = match on_dump {
        Some(cb) => cb,
        None => {
            if let Some(err_cb) = on_error {
                let msg = CString::new("ten_rust_backtrace_dump: on_dump is NULL").unwrap();
                err_cb(ctx, msg.as_ptr(), 0, ptr::null_mut());
            }
            return -1;
        }
    };

    // due to the additional stack frames introduced by FFI bridge, we skip them
    // here to avoid printing the frames of Rust/FFI itself
    // skip 5 frames: (1 and 2 comes from extern crate backtrace)
    // 1. _Unwind_Backtrace() from backtrace::backtrace::libunwind::trace
    // 2. trace_unsynchronized() from backtrace::backtrace::trace
    // 3. ten_rust_backtrace_dump()
    // 4. ten_backtrace_dump()
    // 5. ten_backtrace_dump_global()
    let additional_skip: usize = 5;
    let total_skip = skip.saturating_add(additional_skip);

    let mut frame_index: usize = 0;
    let mut status: c_int = 0;

    backtrace::trace(|frame| {
        // skip the first several frames
        if frame_index < total_skip {
            frame_index += 1;
            return true; // continue to the next frame
        }

        let ip = frame.ip() as usize;

        // parse symbol information: function name, file name and line number
        let mut function_c: Option<CString> = None;
        let mut filename_c: Option<CString> = None;
        let mut lineno_c: c_int = 0;

        backtrace::resolve(frame.ip(), |symbol| {
            if function_c.is_none() {
                if let Some(name) = symbol.name() {
                    // to_string() will do demangle, get the readable function name
                    if let Ok(s) = CString::new(name.to_string()) {
                        function_c = Some(s);
                    }
                }
            }

            if filename_c.is_none() {
                if let Some(path) = symbol.filename() {
                    if let Some(path_str) = path.to_str() {
                        if let Ok(s) = CString::new(path_str) {
                            filename_c = Some(s);
                        }
                    }
                }
            }

            if lineno_c == 0 {
                if let Some(line) = symbol.lineno() {
                    lineno_c = line as c_int;
                }
            }
        });

        let filename_ptr = filename_c.as_ref().map(|s| s.as_ptr()).unwrap_or(ptr::null());
        let function_ptr = function_c.as_ref().map(|s| s.as_ptr()).unwrap_or(ptr::null());

        let rc = on_dump_cb(ctx, ip, filename_ptr, lineno_c, function_ptr, ptr::null_mut());
        if rc != 0 {
            status = rc;
            return false;
        }

        true // continue to iterate the next frame
    });

    status
}
