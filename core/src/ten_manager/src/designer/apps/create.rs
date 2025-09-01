//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{path::Path, sync::Arc};

use actix_web::{web, HttpResponse, Responder};
use anyhow::{anyhow, Result};
use semver::{Version, VersionReq};
use serde::{Deserialize, Serialize};
use ten_rust::pkg_info::pkg_type::PkgType;

use crate::{
    create::create_pkg_in_path,
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    fs::check_is_valid_dir,
    pkg_info::get_all_pkgs::get_all_pkgs_in_app,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct CreateAppRequestPayload {
    pub base_dir: String,
    pub app_name: String,
    pub template_name: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub template_version: Option<Version>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct CreateAppResponseData {
    pub app_path: String,
}

pub async fn create_app_endpoint(
    request_payload: web::Json<CreateAppRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let CreateAppRequestPayload {
        base_dir,
        app_name,
        template_name,
        template_version,
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
        let error_response = ErrorResponse::from_error(&err, "Invalid base directory");
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    // Create app directory.
    let app_path = Path::new(&base_dir).join(&app_name);

    // Check if app directory already exists.
    if app_path.exists() {
        let error_response = ErrorResponse::from_error(
            &anyhow!("App directory already exists"),
            "App directory already exists",
        );
        return Ok(HttpResponse::Conflict().json(error_response));
    }

    // Create app using create_pkg_in_path.
    let version_req = match template_version {
        Some(version) => VersionReq::parse(&format!("={version}")).unwrap(),
        None => VersionReq::default(),
    };

    match create_pkg_in_path(
        state.tman_config.clone(),
        Path::new(&base_dir),
        &PkgType::App,
        &app_name,
        &template_name,
        &version_req,
        None,
        &state.out,
    )
    .await
    {
        Ok(_) => {
            let app_path_str = app_path.to_string_lossy().to_string();

            let mut pkgs_cache = state.pkgs_cache.write().await;
            let mut graphs_cache = state.graphs_cache.write().await;

            // Try to load the newly created app into the cache.
            if let Err(err) =
                get_all_pkgs_in_app(&mut pkgs_cache, &mut graphs_cache, &app_path_str).await
            {
                // Don't delete the app directory on cache update failure.
                let error_response =
                    ErrorResponse::from_error(&err, "App created but failed to update cache");
                return Ok(HttpResponse::Ok().json(error_response));
            }

            let response = ApiResponse {
                status: Status::Ok,
                data: CreateAppResponseData {
                    app_path: app_path_str,
                },
                meta: None,
            };

            Ok(HttpResponse::Created().json(response))
        }
        Err(err) => {
            let error_response = ErrorResponse::from_error(
                &anyhow!("Failed to create app: {}", err),
                "Failed to create app",
            );
            Ok(HttpResponse::InternalServerError().json(error_response))
        }
    }
}
