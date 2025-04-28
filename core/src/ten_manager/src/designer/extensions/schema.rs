//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};

use ten_rust::pkg_info::manifest::api::ManifestApi;

use crate::designer::{
    response::{ApiResponse, ErrorResponse, Status},
    DesignerState,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct GetExtensionSchemaRequestPayload {
    pub app_base_dir: String,
    pub addon_name: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct GetExtensionSchemaResponseData {
    pub schema: Option<ManifestApi>,
}

pub async fn get_extension_schema_endpoint(
    request_payload: web::Json<GetExtensionSchemaRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let pkgs_cache = state.pkgs_cache.read().await;

    // Check if app exists.
    let pkgs_info_in_app = match pkgs_cache.get(&request_payload.app_base_dir) {
        Some(info) => info,
        None => {
            let error_response = ErrorResponse::from_error(
                &anyhow!("App not found"),
                "App not found",
            );
            return Ok(HttpResponse::NotFound().json(error_response));
        }
    };

    // Check if extension info exists.
    let extension_pkgs_info = match &pkgs_info_in_app.extension_pkgs_info {
        Some(info) => info,
        None => {
            let error_response = ErrorResponse::from_error(
                &anyhow!("Extension not found"),
                "Extension not found",
            );
            return Ok(HttpResponse::NotFound().json(error_response));
        }
    };

    // Find specific extension.
    let extension_pkg_info = extension_pkgs_info.iter().find(|pkg_info| {
        pkg_info.manifest.type_and_name.name == request_payload.addon_name
    });

    // Check if extension exists.
    let extension_pkg_info = match extension_pkg_info {
        Some(info) => info,
        None => {
            let error_response = ErrorResponse::from_error(
                &anyhow!("Extension not found"),
                "Extension not found",
            );
            return Ok(HttpResponse::NotFound().json(error_response));
        }
    };

    // Success case.
    let response = ApiResponse {
        status: Status::Ok,
        data: GetExtensionSchemaResponseData {
            schema: extension_pkg_info.manifest.api.clone(),
        },
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
