//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use actix_web::{web, HttpResponse};

// In normal builds, bindings are not used anymore since we pass values directly.
// #[cfg(not(test))]
// use super::bindings::*;

pub fn configure_api_route(
    cfg: &mut web::ServiceConfig,
    runtime_version: String,
    log_path: String,
) {
    cfg.service(
        web::scope("/api/v1")
            .service(web::resource("/version").route(web::get().to(move || {
                let version = runtime_version.clone();
                async move {
                    HttpResponse::Ok().json(web::Json(serde_json::json!({
                        "version": version
                    })))
                }
            })))
            .service(web::resource("/log-path").route(web::get().to(move || {
                let path = log_path.clone();
                async move {
                    HttpResponse::Ok().json(web::Json(serde_json::json!({
                        "log_path": path
                    })))
                }
            }))),
    );
}