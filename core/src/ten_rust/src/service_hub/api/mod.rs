//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::ffi::CStr;

use actix_web::{web, HttpResponse};

// In normal builds, use the implementation from bindings.
#[cfg(not(test))]
use super::bindings::*;

pub fn configure_api_route(cfg: &mut web::ServiceConfig) {
    cfg.service(
        web::scope("/api/v1")
            .service(web::resource("/version").route(web::get().to(|| async {
                let version = unsafe {
                    let c_str = CStr::from_ptr(ten_get_runtime_version());
                    c_str.to_string_lossy().into_owned()
                };

                HttpResponse::Ok().json(web::Json(serde_json::json!({
                    "version": version
                })))
            })))
            .service(web::resource("/log-path").route(web::get().to(move || async move {
                let log_path = unsafe {
                    let c_str = CStr::from_ptr(ten_get_global_log_path());
                    c_str.to_string_lossy().into_owned()
                };

                HttpResponse::Ok().json(web::Json(serde_json::json!({
                    "log_path": log_path
                })))
            }))),
    );
}

#[cfg(test)]
use mock::*;

// When running unit tests, use the mock implementation.
#[cfg(test)]
mod mock {
    use std::os::raw::c_char;

    #[no_mangle]
    pub extern "C" fn ten_get_runtime_version() -> *const c_char {
        "1.0.0".as_ptr() as *const c_char
    }

    #[no_mangle]
    pub extern "C" fn ten_get_global_log_path() -> *const c_char {
        "/tmp/ten_runtime.log".as_ptr() as *const c_char
    }
}
