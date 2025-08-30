//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder, Result};
use serde::{Deserialize, Serialize};

use super::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Serialize, Deserialize)]
pub struct EnvInfo {
    os: String,
    arch: String,
}

pub async fn get_env_endpoint(
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let current_env = ten_rust::env::get_env()
        .map_err(|e| actix_web::error::ErrorInternalServerError(e.to_string()))?;

    // Create response object
    let env_info = EnvInfo {
        os: current_env.os.to_string(),
        arch: current_env.arch.to_string(),
    };

    let response = ApiResponse {
        status: Status::Ok,
        data: env_info,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
