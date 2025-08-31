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

use super::{read_persistent_storage, write_persistent_storage};
use crate::designer::{
    response::{ApiResponse, Status},
    storage::in_memory::key_parser::set_value_by_key,
    DesignerState,
};

#[derive(Debug, Serialize, Deserialize)]
pub struct SetPersistentRequestPayload {
    pub key: String,
    pub value: Value,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SetPersistentResponseData {
    pub success: bool,
}

pub async fn set_persistent_storage_endpoint(
    request_payload: web::Json<SetPersistentRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let payload = request_payload.into_inner();
    let key = payload.key;
    let value = payload.value;

    // Check if schema has been set and validate the key/value
    {
        let schema_lock = state.persistent_storage_schema.read().await;
        if let Some(validator) = schema_lock.as_ref() {
            // Create a temporary object to validate the key/value pair
            let mut temp_data = serde_json::json!({});
            if let Err(_e) = set_value_by_key(&mut temp_data, &key, value.clone()) {
                return Ok(HttpResponse::BadRequest().json(ApiResponse {
                    status: Status::Fail,
                    data: SetPersistentResponseData {
                        success: false,
                    },
                    meta: None,
                }));
            }

            // Validate the value against the schema
            if let Err(_e) = validator.validate(&temp_data) {
                let mut error_messages = Vec::new();
                for error in validator.iter_errors(&temp_data) {
                    error_messages.push(format!("{} @ {}", error, error.instance_path));
                }
                return Ok(HttpResponse::BadRequest().json(ApiResponse {
                    status: Status::Fail,
                    data: SetPersistentResponseData {
                        success: false,
                    },
                    meta: None,
                }));
            }
        } else {
            return Ok(HttpResponse::BadRequest().json(ApiResponse {
                status: Status::Fail,
                data: SetPersistentResponseData {
                    success: false,
                },
                meta: None,
            }));
        }
    }

    // Read current persistent storage data
    let mut json_data = match read_persistent_storage() {
        Ok(data) => data,
        Err(_e) => {
            return Ok(HttpResponse::InternalServerError().json(ApiResponse {
                status: Status::Fail,
                data: SetPersistentResponseData {
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
            data: SetPersistentResponseData {
                success: false,
            },
            meta: None,
        }));
    }

    // Write the updated data back to disk
    if let Err(_e) = write_persistent_storage(&json_data) {
        return Ok(HttpResponse::InternalServerError().json(ApiResponse {
            status: Status::Fail,
            data: SetPersistentResponseData {
                success: false,
            },
            meta: None,
        }));
    }

    let response_data = SetPersistentResponseData {
        success: true,
    };
    let response = ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
