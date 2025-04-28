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

use crate::designer::response::{ApiResponse, Status};
use crate::designer::DesignerState;
use crate::schema::validate_designer_config;

use super::save_config_to_file;

#[derive(Debug, Serialize, Deserialize)]
pub struct UpdatePreferencesFieldRequestPayload {
    pub field: String,
    pub value: Value,
}

/// Update a specific field in designer frontend preferences.
pub async fn update_preferences_field_endpoint(
    request_payload: web::Json<UpdatePreferencesFieldRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut tman_config = state.tman_config.write().await;

    // Update specific field in designer frontend preferences.
    let mut designer_value = serde_json::to_value(&tman_config.designer)
        .map_err(actix_web::error::ErrorInternalServerError)?;

    if let Some(obj) = designer_value.as_object_mut() {
        obj.insert(
            request_payload.field.clone(),
            request_payload.value.clone(),
        );
    } else {
        return Err(actix_web::error::ErrorBadRequest(
            "Designer is not an object",
        ));
    }

    // Validate against schema.
    if let Err(e) = validate_designer_config(&designer_value) {
        return Err(actix_web::error::ErrorBadRequest(e.to_string()));
    }

    tman_config.designer = serde_json::from_value(designer_value)
        .map_err(actix_web::error::ErrorBadRequest)?;

    // Save to config file.
    save_config_to_file(&mut tman_config)?;

    let response = ApiResponse { status: Status::Ok, data: (), meta: None };

    Ok(HttpResponse::Ok().json(response))
}
