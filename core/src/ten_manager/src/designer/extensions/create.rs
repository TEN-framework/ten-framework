//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{path::Path, sync::Arc};

use actix_web::{web, HttpResponse, Responder};
use anyhow::{anyhow, Result};
use semver::VersionReq;
use serde::{Deserialize, Serialize};

use crate::{
    create::create_pkg_in_path,
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    fs::check_is_valid_dir,
    pkg_info::get_all_pkgs::get_all_pkgs_in_app,
};

use ten_rust::pkg_info::pkg_type::PkgType;

#[derive(Deserialize, Serialize, Debug)]
pub struct CreateExtensionRequestPayload {
    pub base_dir: String,
    pub template_name: String,
    pub extension_name: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct CreateExtensionResponseData {
    pub extension_path: String,
}

pub async fn create_extension_endpoint(
    request_payload: web::Json<CreateExtensionRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let CreateExtensionRequestPayload {
        base_dir,
        extension_name,
        template_name,
    } = request_payload.into_inner();

    // Validate base_dir exists.
    if !Path::new(&base_dir).exists() {
        let error_response = ErrorResponse::from_error(
            &anyhow!("Base directory does not exist"),
            "Base directory does not exist",
        );
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    // Validate base_dir is a directory.
    if let Err(err) = check_is_valid_dir(Path::new(&base_dir)) {
        let error_response =
            ErrorResponse::from_error(&err, "Invalid base directory");
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    // Create extension directory.
    let extension_path = Path::new(&base_dir).join(&extension_name);

    // Check if extension directory already exists.
    if extension_path.exists() {
        let error_response = ErrorResponse::from_error(
            &anyhow!("Extension directory already exists"),
            "Extension directory already exists",
        );
        return Ok(HttpResponse::Conflict().json(error_response));
    }

    // Create extension using create_pkg_in_path.
    match create_pkg_in_path(
        state.tman_config.clone(),
        Path::new(&base_dir),
        &PkgType::Extension,
        &extension_name,
        &template_name,
        &VersionReq::default(),
        None,
        &state.out,
    )
    .await
    {
        Ok(_) => {
            let extension_path_str =
                extension_path.to_string_lossy().to_string();

            let mut pkgs_cache = state.pkgs_cache.write().await;
            let mut graphs_cache = state.graphs_cache.write().await;

            // Try to load the newly created extension into the cache.
            if let Err(err) = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &extension_path_str,
            ) {
                // Don't delete the extension directory on cache update failure.
                let error_response = ErrorResponse::from_error(
                    &err,
                    "Extension created but failed to update cache",
                );
                return Ok(HttpResponse::Ok().json(error_response));
            }

            let response = ApiResponse {
                status: Status::Ok,
                data: CreateExtensionResponseData {
                    extension_path: extension_path_str,
                },
                meta: None,
            };

            Ok(HttpResponse::Created().json(response))
        }
        Err(err) => {
            let error_response = ErrorResponse::from_error(
                &anyhow!("Failed to create extension: {}", err),
                "Failed to create extension",
            );
            Ok(HttpResponse::InternalServerError().json(error_response))
        }
    }
}
