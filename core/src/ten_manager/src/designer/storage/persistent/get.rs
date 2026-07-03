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

use super::read_persistent_storage;
use crate::designer::{
    response::{ApiResponse, Status},
    storage::in_memory::key_parser::get_value_by_key,
    DesignerState,
};

#[derive(Debug, Serialize, Deserialize)]
pub struct GetPersistentRequestPayload {
    pub key: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct GetPersistentResponseData {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub value: Option<Value>,
}

pub async fn get_persistent_storage_endpoint(
    request_payload: web::Json<GetPersistentRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let key = request_payload.key.clone();

    // Check if schema has been set
    {
        let schema_lock = state.persistent_storage_schema.read().await;
        if schema_lock.is_none() {
            return Ok(HttpResponse::BadRequest().json(ApiResponse {
                status: Status::Fail,
                data: GetPersistentResponseData {
                    value: None,
                },
                meta: None,
            }));
        }
    }

    // Read persistent storage data from disk
    let json_data = match read_persistent_storage() {
        Ok(data) => data,
        Err(_e) => {
            return Ok(HttpResponse::InternalServerError().json(ApiResponse {
                status: Status::Fail,
                data: GetPersistentResponseData {
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
                data: GetPersistentResponseData {
                    value: None,
                },
                meta: None,
            }));
        }
    };

    let response_data = GetPersistentResponseData {
        value,
    };
    let response = ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
