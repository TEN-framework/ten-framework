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

use super::key_parser::set_value_by_key;
use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Debug, Serialize, Deserialize)]
pub struct SetMemoryRequestPayload {
    pub key: String,
    pub value: Value,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SetMemoryResponseData {
    pub success: bool,
}

pub async fn set_in_memory_storage_endpoint(
    request_payload: web::Json<SetMemoryRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let payload = request_payload.into_inner();
    let key = payload.key;
    let value = payload.value;

    // Get the current metadata
    let mut metadata_guard = state.storage_in_memory.write().await;

    // Convert the metadata to JSON for manipulation
    let mut json_data = match serde_json::to_value(&*metadata_guard) {
        Ok(data) => data,
        Err(_e) => {
            return Ok(HttpResponse::InternalServerError().json(ApiResponse {
                status: Status::Fail,
                data: SetMemoryResponseData {
                    success: false,
                },
                meta: None,
            }));
        }
    };

    // Set the value using the key parser
    if let Err(_e) = set_value_by_key(&mut json_data, &key, value) {
        return Ok(HttpResponse::BadRequest().json(ApiResponse {
            status: Status::Fail,
            data: SetMemoryResponseData {
                success: false,
            },
            meta: None,
        }));
    }

    // Convert back to TmanStorageInMemory
    match serde_json::from_value(json_data) {
        Ok(new_metadata) => {
            *metadata_guard = new_metadata;
        }
        Err(_e) => {
            return Ok(HttpResponse::InternalServerError().json(ApiResponse {
                status: Status::Fail,
                data: SetMemoryResponseData {
                    success: false,
                },
                meta: None,
            }));
        }
    }

    let response_data = SetMemoryResponseData {
        success: true,
    };
    let response = ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
