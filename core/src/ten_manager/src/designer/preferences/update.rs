//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use serde_json;

use crate::designer::response::{ApiResponse, Status};
use crate::designer::DesignerState;
use crate::schema::validate_designer_config;

use super::{save_config_to_file, Designer};

#[derive(Debug, Serialize, Deserialize)]
pub struct UpdatePreferencesRequestPayload {
    pub preferences: Designer,
}

/// Update the full content of designer frontend preferences.
pub async fn update_preferences_endpoint(
    request_payload: web::Json<UpdatePreferencesRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    // Convert Designer to Value for validation.
    let mut tman_config = state.tman_config.write().await;

    let preferences_value = serde_json::to_value(&request_payload.preferences)
        .map_err(actix_web::error::ErrorBadRequest)?;

    // Validate against schema.
    if let Err(e) = validate_designer_config(&preferences_value) {
        return Err(actix_web::error::ErrorBadRequest(e.to_string()));
    }

    // Update designer field.
    tman_config.designer = request_payload.preferences.clone();

    // Save to config file
    save_config_to_file(&mut tman_config)?;

    let response = ApiResponse { status: Status::Ok, data: (), meta: None };

    Ok(HttpResponse::Ok().json(response))
}
