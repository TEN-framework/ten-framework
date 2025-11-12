//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use super::{
    response::{ApiResponse, Status},
    DesignerState,
};
use crate::{constants::GITHUB_RELEASE_PAGE, version::VERSION, version_utils::check_update};

#[derive(Serialize, Deserialize, PartialEq, Debug)]
struct GetVersionResponseData {
    version: String,
}

#[derive(Serialize, Deserialize, PartialEq, Debug)]
struct CheckUpdateResponseData {
    update_available: bool,
    latest_version: Option<String>,
    release_page: Option<String>,
    message: Option<String>,
}

pub async fn get_version_endpoint(
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let version_info = GetVersionResponseData {
        version: VERSION.to_string(),
    };

    let response = ApiResponse {
        status: Status::Ok,
        data: version_info,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}

pub async fn check_update_endpoint() -> Result<impl Responder, actix_web::Error> {
    match check_update().await {
        Ok((true, latest)) => {
            let update_info = CheckUpdateResponseData {
                update_available: true,
                latest_version: Some(latest),
                release_page: Some(GITHUB_RELEASE_PAGE.to_string()),
                message: None,
            };
            let response = ApiResponse {
                status: Status::Ok,
                data: update_info,
                meta: None,
            };
            Ok(HttpResponse::Ok().json(response))
        }
        Ok((false, _)) => {
            let update_info = CheckUpdateResponseData {
                update_available: false,
                latest_version: None,
                release_page: None,
                message: None,
            };
            let response = ApiResponse {
                status: Status::Ok,
                data: update_info,
                meta: None,
            };
            Ok(HttpResponse::Ok().json(response))
        }
        Err(err_msg) => {
            let update_info = CheckUpdateResponseData {
                update_available: false,
                latest_version: None,
                release_page: None,
                message: Some(err_msg),
            };
            let response = ApiResponse {
                status: Status::Fail,
                data: update_info,
                meta: None,
            };
            Ok(HttpResponse::Ok().json(response))
        }
    }
}
