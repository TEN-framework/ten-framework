//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::graphs_cache_remove_by_app_base_dir,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct UnloadAppRequestPayload {
    pub base_dir: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct UnloadAppResponseData {
    pub success: bool,
}

pub async fn unload_app_endpoint(
    request_payload: web::Json<UnloadAppRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut pkgs_cache = state.pkgs_cache.write().await;
    let mut graphs_cache = state.graphs_cache.write().await;

    // Check if the base_dir exists in pkgs_cache.
    if !pkgs_cache.contains_key(&request_payload.base_dir) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!(
                "App with base_dir '{}' is not loaded",
                request_payload.base_dir
            ),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    // Remove the app from pkgs_cache.
    pkgs_cache.remove(&request_payload.base_dir);

    // Remove any graphs associated with this app.
    let base_dir = &request_payload.base_dir;
    graphs_cache_remove_by_app_base_dir(&mut graphs_cache, base_dir);

    let response = ApiResponse {
        status: Status::Ok,
        data: serde_json::json!({ "success": true }),
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
