//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use anyhow::Result;
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

use super::{read_persistent_storage, write_persistent_storage};
use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Debug, Serialize, Deserialize)]
pub struct SetSchemaRequestPayload {
    pub schema: Value,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SetSchemaResponseData {
    pub success: bool,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub cleaned_fields: Option<Vec<String>>,
}

/// Validates and cleans persistent storage data against the provided schema
///
/// Currently uses a simple validation that checks if the top-level fields of
/// persistent storage satisfy the schema. Fields that don't match are removed
/// entirely. In the future, we may optimize this validation logic based on
/// requirements, which might require more time to complete the validation.
fn validate_and_clean_storage_data(data: &mut Value, schema: &Value) -> Result<Vec<String>> {
    let mut cleaned_fields = Vec::new();

    if let Some(obj) = data.as_object_mut() {
        let mut keys_to_remove = Vec::new();

        for (key, value) in obj.iter() {
            // Check if this field has a corresponding schema
            if let Some(schema_props) = schema.get("properties").and_then(|p| p.as_object()) {
                if let Some(field_schema) = schema_props.get(key) {
                    // Validate the field value against its schema
                    let field_validator = match jsonschema::validator_for(field_schema) {
                        Ok(v) => v,
                        Err(_) => continue, // Skip if schema is invalid
                    };

                    if field_validator.validate(value).is_err() {
                        // Field doesn't match schema, mark for removal
                        keys_to_remove.push(key.clone());
                        cleaned_fields.push(key.clone());
                    }
                } else {
                    // No schema for this field, remove it
                    keys_to_remove.push(key.clone());
                    cleaned_fields.push(key.clone());
                }
            }
        }

        // Remove invalid fields
        for key in keys_to_remove {
            obj.remove(&key);
        }

        // Add default values for required fields that are missing
        if let Some(schema_props) = schema.get("properties").and_then(|p| p.as_object()) {
            if let Some(required_fields) = schema.get("required").and_then(|r| r.as_array()) {
                for required_field in required_fields {
                    if let Some(field_name) = required_field.as_str() {
                        if !obj.contains_key(field_name) {
                            if let Some(field_schema) = schema_props.get(field_name) {
                                if let Some(default_value) = get_default_value(field_schema) {
                                    obj.insert(field_name.to_string(), default_value);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Ok(cleaned_fields)
}

/// Get default value for a schema field
fn get_default_value(schema: &Value) -> Option<Value> {
    // Check if schema has a default value
    if let Some(default) = schema.get("default") {
        return Some(default.clone());
    }

    // Generate default based on type
    if let Some(type_val) = schema.get("type") {
        if let Some(type_str) = type_val.as_str() {
            return match type_str {
                "string" => Some(Value::String("".to_string())),
                "number" => Some(Value::Number(serde_json::Number::from(0))),
                "integer" => Some(Value::Number(serde_json::Number::from(0))),
                "boolean" => Some(Value::Bool(false)),
                "array" => Some(Value::Array(Vec::new())),
                "object" => Some(Value::Object(Map::new())),
                _ => Some(Value::Null),
            };
        }
    }

    Some(Value::Null)
}

pub async fn set_persistent_storage_schema_endpoint(
    request_payload: web::Json<SetSchemaRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let payload = request_payload.into_inner();
    let schema = payload.schema;

    // Validate that the provided schema is a valid JSON schema
    let validator = match jsonschema::validator_for(&schema) {
        Ok(v) => v,
        Err(_e) => {
            return Ok(HttpResponse::BadRequest().json(ApiResponse {
                status: Status::Fail,
                data: SetSchemaResponseData {
                    success: false,
                    cleaned_fields: None,
                },
                meta: None,
            }));
        }
    };

    // Read current persistent storage data
    let mut storage_data = match read_persistent_storage() {
        Ok(data) => data,
        Err(_e) => {
            return Ok(HttpResponse::InternalServerError().json(ApiResponse {
                status: Status::Fail,
                data: SetSchemaResponseData {
                    success: false,
                    cleaned_fields: None,
                },
                meta: None,
            }));
        }
    };

    // Validate and clean existing data against the new schema
    let cleaned_fields = match validate_and_clean_storage_data(&mut storage_data, &schema) {
        Ok(fields) => fields,
        Err(_e) => {
            return Ok(HttpResponse::InternalServerError().json(ApiResponse {
                status: Status::Fail,
                data: SetSchemaResponseData {
                    success: false,
                    cleaned_fields: None,
                },
                meta: None,
            }));
        }
    };

    // Write the cleaned data back to storage
    if let Err(_e) = write_persistent_storage(&storage_data) {
        return Ok(HttpResponse::InternalServerError().json(ApiResponse {
            status: Status::Fail,
            data: SetSchemaResponseData {
                success: false,
                cleaned_fields: None,
            },
            meta: None,
        }));
    }

    // Store the schema in memory for future validations
    {
        let mut schema_lock = state.persistent_storage_schema.write().await;
        *schema_lock = Some(validator);
    }

    let response_data = SetSchemaResponseData {
        success: true,
        cleaned_fields: if cleaned_fields.is_empty() { None } else { Some(cleaned_fields) },
    };

    Ok(HttpResponse::Ok().json(ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    }))
}
