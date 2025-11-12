//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    registry,
    registry::{found_result::PkgRegistryInfo, search::PkgSearchFilter},
};

#[derive(Deserialize, Serialize, Debug)]
pub struct PkgSearchOptions {
    pub page_size: Option<u32>,
    pub page: Option<u32>,
    pub sort_by: Option<String>,
    pub sort_order: Option<String>,
    pub scope: Option<String>,
}

#[derive(Deserialize, Serialize, Debug)]
pub struct SearchPackagesRequestPayload {
    pub filter: PkgSearchFilter,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub options: Option<PkgSearchOptions>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct SearchPackagesResponseData {
    pub total_size: u32,

    pub packages: Vec<PkgRegistryInfo>,
}

pub async fn search_packages_endpoint(
    request_payload: web::Json<SearchPackagesRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    match registry::search_packages(
        state.tman_config.clone(),
        &request_payload.filter,
        request_payload.options.as_ref().and_then(|o| o.page_size),
        request_payload.options.as_ref().and_then(|o| o.page),
        request_payload.options.as_ref().and_then(|o| o.sort_by.as_deref()),
        request_payload.options.as_ref().and_then(|o| o.sort_order.as_deref()),
        request_payload.options.as_ref().and_then(|o| o.scope.as_deref()),
        &state.out,
    )
    .await
    {
        Ok((total_size, packages)) => {
            let response_data = SearchPackagesResponseData {
                total_size,
                packages,
            };

            Ok(HttpResponse::Ok().json(ApiResponse {
                status: Status::Ok,
                data: response_data,
                meta: None,
            }))
        }
        Err(err) => {
            let error_response = ErrorResponse::from_error(&err, "Failed to search packages");
            Ok(HttpResponse::InternalServerError().json(error_response))
        }
    }
}
