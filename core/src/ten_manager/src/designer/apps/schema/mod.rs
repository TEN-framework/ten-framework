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
pub struct GetAppSchemaRequestPayload {
    pub app_base_dir: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct GetAppSchemaResponseData {
    pub schema: Option<ManifestApi>,
}

pub async fn get_app_schema_endpoint(
    request_payload: web::Json<GetAppSchemaRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let pkgs_cache = state.pkgs_cache.read().await;

    if let Some(pkgs_info_in_app) =
        pkgs_cache.get(&request_payload.app_base_dir)
    {
        if let Some(app_pkg_info) = &pkgs_info_in_app.app_pkg_info {
            let response = ApiResponse {
                status: Status::Ok,
                data: GetAppSchemaResponseData {
                    schema: app_pkg_info.manifest.api.clone(),
                },
                meta: None,
            };

            Ok(HttpResponse::Ok().json(response))
        } else {
            let error_response = ErrorResponse::from_error(
                &anyhow!("App not found"),
                "App not found",
            );
            Ok(HttpResponse::NotFound().json(error_response))
        }
    } else {
        let error_response = ErrorResponse::from_error(
            &anyhow!("App not found"),
            "App not found",
        );
        Ok(HttpResponse::NotFound().json(error_response))
    }
}
