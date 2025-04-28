//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;
use std::path::Path;
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use anyhow::Result;
use serde::{Deserialize, Serialize};
use ten_rust::base_dir_pkg_info::PkgsInfoInApp;

use crate::{
    designer::response::{ApiResponse, ErrorResponse, Status},
    designer::DesignerState,
    fs::check_is_app_folder,
    pkg_info::get_all_pkgs::get_all_pkgs_in_app,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct LoadAppRequestPayload {
    pub base_dir: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct LoadAppResponseData {
    pub app_uri: Option<String>,
}

pub async fn load_app_endpoint(
    request_payload: web::Json<LoadAppRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut pkgs_cache = state.pkgs_cache.write().await;
    let mut graphs_cache = state.graphs_cache.write().await;

    if pkgs_cache.contains_key(&request_payload.base_dir) {
        let app_uri = extract_app_uri(&pkgs_cache, &request_payload.base_dir);
        return Ok(HttpResponse::Ok().json(ApiResponse {
            status: Status::Ok,
            data: LoadAppResponseData { app_uri },
            meta: None,
        }));
    }

    match check_is_app_folder(Path::new(&request_payload.base_dir)) {
        Ok(_) => {
            if let Err(err) = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &request_payload.base_dir,
            ) {
                let error_response =
                    ErrorResponse::from_error(&err, "Error fetching packages:");
                return Ok(HttpResponse::NotFound().json(error_response));
            }

            let app_uri =
                extract_app_uri(&pkgs_cache, &request_payload.base_dir);
            Ok(HttpResponse::Ok().json(ApiResponse {
                status: Status::Ok,
                data: LoadAppResponseData { app_uri },
                meta: None,
            }))
        }
        Err(err) => {
            let error_response = ErrorResponse::from_error(
                &err,
                format!("{} is not an app folder: ", &request_payload.base_dir)
                    .as_str(),
            );
            Ok(HttpResponse::NotFound().json(error_response))
        }
    }
}

fn extract_app_uri(
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    base_dir: &str,
) -> Option<String> {
    pkgs_cache.get(base_dir).and_then(|base_dir_pkg_info| {
        // Check the app package first.
        if let Some(app_pkg) = &base_dir_pkg_info.app_pkg_info {
            if let Some(property) = &app_pkg.property {
                if let Some(ten) = &property.ten {
                    if let Some(uri) = &ten.uri {
                        return Some(uri.to_string());
                    }
                }
            }
        }

        None
    })
}
