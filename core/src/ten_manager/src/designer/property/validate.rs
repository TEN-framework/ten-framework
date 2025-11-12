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
use ten_rust::json_schema::ten_validate_property_json_string;

use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Deserialize, Serialize, Debug)]
pub struct ValidatePropertyRequestPayload {
    pub property_json_str: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct ValidatePropertyResponseData {
    pub is_valid: bool,
    pub error_message: Option<String>,
}

pub async fn validate_property_endpoint(
    request_payload: web::Json<ValidatePropertyRequestPayload>,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let property_json_str = &request_payload.property_json_str;

    let response_data = match ten_validate_property_json_string(property_json_str) {
        Ok(_) => ValidatePropertyResponseData {
            is_valid: true,
            error_message: None,
        },
        Err(err) => ValidatePropertyResponseData {
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
