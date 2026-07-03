//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, Responder};
use anyhow::Result;
use serde::{Deserialize, Serialize};
use ten_rust::json_schema::ten_validate_manifest_json_string;

use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct ValidateManifestRequestPayload {
    pub manifest_json_str: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct ValidateManifestResponseData {
    pub is_valid: bool,
    pub error_message: Option<String>,
}

pub async fn validate_manifest_endpoint(
    request_payload: web::Json<ValidateManifestRequestPayload>,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let manifest_json_str = &request_payload.manifest_json_str;

    let response_data = match ten_validate_manifest_json_string(manifest_json_str) {
        Ok(_) => ValidateManifestResponseData {
            is_valid: true,
            error_message: None,
        },
        Err(err) => ValidateManifestResponseData {
            is_valid: false,
            error_message: Some(err.to_string()),
        },
    };

    let api_response = ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    };

    Ok(web::Json(api_response))
}
