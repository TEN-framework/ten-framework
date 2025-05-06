//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};
use std::sync::OnceLock;

use crate::designer::{
    response::{ApiResponse, ErrorResponse, Status},
    DesignerState,
};

#[derive(Debug, Deserialize, Serialize)]
pub struct GetEnvVarRequestPayload {
    pub name: String,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct GetEnvVarResponseData {
    pub value: String,
}

/// This function handles requests for help text from the frontend. It accepts a
/// JSON payload with a "key" property and returns the corresponding help text.
pub async fn get_env_var_endpoint(
    request_payload: web::Json<GetEnvVarRequestPayload>,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let name = &request_payload.name;

    let value = "test";

    let response_data = GetEnvVarResponseData { value: value.to_string() };

    Ok(HttpResponse::Ok().json(response_data))
}
