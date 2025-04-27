//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use actix_web::{HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::designer::response::{ApiResponse, Status};
use crate::schema::get_designer_schema;

#[derive(Debug, Serialize, Deserialize)]
pub struct GetPreferencesSchemaResponseData {
    pub schema: Value,
}

/// Get the JSON schema for designer frontend preferences.
pub async fn get_preferences_schema_endpoint(
) -> Result<impl Responder, actix_web::Error> {
    let schema: Value = serde_json::from_str(get_designer_schema())
        .map_err(actix_web::error::ErrorInternalServerError)?;

    let response_data = GetPreferencesSchemaResponseData { schema };
    let response =
        ApiResponse { status: Status::Ok, data: response_data, meta: None };

    Ok(HttpResponse::Ok().json(response))
}
