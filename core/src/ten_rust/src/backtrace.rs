//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use backtrace;
use core::ffi::{c_char, c_int, c_void};
use std::ffi::CString;
use std::ptr;

///This function is a wrapper for the backtrace::trace and backtrace::resolve functions.
///It is used to dump the backtrace of the current function.
///It is called by the C function ten_rust_backtrace_dump.
#[no_mangle]
pub extern "C" fn ten_rust_backtrace_dump(
    ctx: *mut c_void,
    on_dump: Option<extern "C" fn(
        ctx: *mut c_void,
        pc: usize,
        filename: *const c_char,
        lineno_c: c_int,
        function: *const c_char,
        data: *mut c_void,
    ) -> c_int>,
    on_error: Option<extern "C" fn(
        ctx: *mut c_void,
        msg: *const c_char,
        errnum: c_int,
        data: *mut c_void,
    )>,
    skip: usize,
) -> c_int {
    // on_dump 为必须参数：没有它就无法把每一帧回调给 C 侧
    let on_dump_cb = match on_dump {
        Some(cb) => cb,
        None => {
            if let Some(err_cb) = on_error {
                let msg = CString::new("ten_rust_backtrace_dump: on_dump is NULL").unwrap();
                // errnum 传 0，以表明非系统错误码
                err_cb(ctx, msg.as_ptr(), 0, ptr::null_mut());
            }
            return -1;
        }
    };

    // 由于 FFI 桥接会额外引入若干栈帧，这里做一个额外跳过，避免把 Rust/FFI 自身的帧打印出来
    let additional_skip: usize = 2;
    let total_skip = skip.saturating_add(additional_skip);

    let mut frame_index: usize = 0;
    let mut status: c_int = 0;

    backtrace::trace(|frame| {
        // 跳过开头若干帧
        if frame_index < total_skip {
            frame_index += 1;
            return true; // 继续下一帧
        }

        let ip = frame.ip() as usize;

        // 解析符号信息：函数名、文件名与行号
        let mut function_c: Option<CString> = None;
        let mut filename_c: Option<CString> = None;
        let mut lineno_c: c_int = 0;

        backtrace::resolve(frame.ip(), |symbol| {

            if function_c.is_none() {
                if let Some(name) = symbol.name() {
                    // to_string() 会做 demangle，得到可读的函数名
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

        // data 传 NULL；如需传递上下文，可在 C 侧通过 self->... 扩展
        let rc = on_dump_cb(ctx, ip, filename_ptr, lineno_c, function_ptr, ptr::null_mut());
        if rc != 0 {
            status = rc;
            return false; // 非 0 表示回调希望中断遍历
        }

        true // 继续遍历下一帧
    });

    status
}