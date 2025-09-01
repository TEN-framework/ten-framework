//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use semver::VersionReq;
use serde::{Deserialize, Serialize};
use ten_rust::pkg_info::pkg_type::PkgType;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    registry,
    registry::found_result::PkgRegistryInfo,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct GetPackagesRequestPayload {
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub pkg_type: Option<PkgType>,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub name: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub version_req: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub page_size: Option<u32>,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub page: Option<u32>,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub tags: Option<Vec<String>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub scope: Option<String>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct GetPackagesResponseData {
    pub packages: Vec<PkgRegistryInfo>,
}

pub async fn get_packages_endpoint(
    request_query: web::Query<GetPackagesRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    // Parse version requirement if provided.
    let version_req = if let Some(version_req_str) = &request_query.version_req {
        match VersionReq::parse(version_req_str) {
            Ok(req) => Some(req),
            Err(e) => {
                return Ok(HttpResponse::BadRequest().json(ErrorResponse {
                    status: Status::Fail,
                    message: format!("Invalid version requirement: {e}"),
                    error: None,
                }));
            }
        }
    } else {
        None
    };

    // Parse scope if provided.
    let scope = request_query
        .scope
        .as_ref()
        .map(|scope_str| scope_str.split(',').map(|s| s.to_string()).collect());

    // Call the registry function to get package list with optional parameters.
    match registry::get_package_list(
        state.tman_config.clone(),
        request_query.pkg_type,
        request_query.name.clone(),
        version_req,
        request_query.tags.clone(),
        scope,
        request_query.page_size,
        request_query.page,
        &state.out,
    )
    .await
    {
        Ok(packages) => {
            let response_data = GetPackagesResponseData {
                packages,
            };

            Ok(HttpResponse::Ok().json(ApiResponse {
                status: Status::Ok,
                data: response_data,
                meta: None,
            }))
        }
        Err(e) => Ok(HttpResponse::InternalServerError().json(ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to get packages: {e}"),
            error: None,
        })),
    }
}
