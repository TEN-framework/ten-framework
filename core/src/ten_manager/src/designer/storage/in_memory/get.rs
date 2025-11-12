//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use serde_json::Value;

use super::key_parser::get_value_by_key;
use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Debug, Serialize, Deserialize)]
pub struct GetMemoryRequestPayload {
    pub key: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct GetMemoryResponseData {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub value: Option<Value>,
}

pub async fn get_in_memory_storage_endpoint(
    request_payload: web::Json<GetMemoryRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let key = request_payload.key.clone();

    // Get the current metadata
    let metadata_guard = state.storage_in_memory.read().await;

    // Convert the metadata to JSON for reading
    let json_data = match serde_json::to_value(&*metadata_guard) {
        Ok(data) => data,
        Err(_e) => {
            return Ok(HttpResponse::InternalServerError().json(ApiResponse {
                status: Status::Fail,
                data: GetMemoryResponseData {
                    value: None,
                },
                meta: None,
            }));
        }
    };

    // Get the value using the key parser
    let value = match get_value_by_key(&json_data, &key) {
        Ok(val) => val,
        Err(_e) => {
            return Ok(HttpResponse::BadRequest().json(ApiResponse {
                status: Status::Fail,
                data: GetMemoryResponseData {
                    value: None,
                },
                meta: None,
            }));
        }
    };

    let response_data = GetMemoryResponseData {
        value,
    };
    let response = ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
