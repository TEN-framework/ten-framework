//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::graphs_cache_remove_by_app_base_dir,
    pkg_info::get_all_pkgs::get_all_pkgs_in_app,
};
use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
pub struct ReloadPkgsRequestPayload {
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub base_dir: Option<String>,
}

pub async fn reload_app_endpoint(
    request_payload: web::Json<ReloadPkgsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut pkgs_cache = state.pkgs_cache.write().await;
    let mut graphs_cache = state.graphs_cache.write().await;

    if let Some(base_dir) = &request_payload.base_dir {
        // Case 1: base_dir is specified in the request payload.

        // Check if the base_dir exists in pkgs_cache.
        if !pkgs_cache.contains_key(base_dir) {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: format!(
                    "App with base_dir '{}' is not loaded",
                    base_dir
                ),
                error: None,
            };
            return Ok(HttpResponse::BadRequest().json(error_response));
        }

        // Clear the existing packages for this base_dir.
        pkgs_cache.remove(base_dir);

        // Remove graphs associated with this app from graphs_cache.
        graphs_cache_remove_by_app_base_dir(&mut graphs_cache, base_dir);

        // Reload packages for this base_dir.
        if let Err(err) =
            get_all_pkgs_in_app(&mut pkgs_cache, &mut graphs_cache, base_dir)
        {
            return Ok(HttpResponse::InternalServerError().json(
                ErrorResponse::from_error(&err, "Failed to reload packages:"),
            ));
        }
    } else {
        // Case 2: base_dir is not specified, reload all apps.

        if pkgs_cache.is_empty() {
            return Ok(HttpResponse::Ok().json(ApiResponse {
                status: Status::Ok,
                data: "No apps to reload",
                meta: None,
            }));
        }

        // Collect all base_dirs first (to avoid borrowing issues).
        let base_dirs: Vec<String> = pkgs_cache.keys().cloned().collect();

        // For each base_dir, clear its packages and reload them.
        for base_dir in base_dirs {
            // Clear the existing packages for this base_dir.
            pkgs_cache.remove(&base_dir);

            // Remove graphs associated with this app from graphs_cache.
            graphs_cache_remove_by_app_base_dir(&mut graphs_cache, &base_dir);

            // Reload packages for this base_dir.
            if let Err(err) = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &base_dir,
            ) {
                return Ok(HttpResponse::InternalServerError().json(
                    ErrorResponse::from_error(
                        &err,
                        "Failed to reload packages:",
                    ),
                ));
            }
        }
    }

    Ok(HttpResponse::Ok().json(ApiResponse {
        status: Status::Ok,
        data: "Packages reloaded successfully",
        meta: None,
    }))
}
