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

use crate::designer::{
    response::{ApiResponse, ErrorResponse, Status},
    DesignerState,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct GetPackagesScriptsRequestPayload {
    pub base_dir: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct GetPackagesScriptsResponseData {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scripts: Option<Vec<String>>,
}

pub async fn get_app_scripts_endpoint(
    request_payload: web::Json<GetPackagesScriptsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let pkgs_cache = state.pkgs_cache.read().await;

    if let Some(base_dir_pkg_info) = pkgs_cache.get(&request_payload.base_dir) {
        if let Some(app_pkg) = &base_dir_pkg_info.app_pkg_info {
            let scripts = app_pkg
                .manifest
                .scripts
                .as_ref()
                .map(|scripts| scripts.keys().cloned().collect());

            let response = ApiResponse {
                status: Status::Ok,
                data: GetPackagesScriptsResponseData { scripts },
                meta: None,
            };

            Ok(HttpResponse::Ok().json(response))
        } else {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: "App package not found".to_string(),
                error: None,
            };
            Ok(HttpResponse::NotFound().json(error_response))
        }
    } else {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: "Package information is missing".to_string(),
            error: None,
        };
        Ok(HttpResponse::NotFound().json(error_response))
    }
}
