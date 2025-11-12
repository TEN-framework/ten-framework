//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{env, sync::Arc};

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use crate::designer::DesignerState;

#[derive(Debug, Deserialize, Serialize)]
pub struct GetEnvVarRequestPayload {
    pub name: String,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct GetEnvVarResponseData {
    pub value: Option<String>,
}

/// This function handles requests for getting environment variables.
/// It accepts a JSON payload with a "name" property and returns the
/// corresponding environment variable value, or None if not defined.
pub async fn get_env_var_endpoint(
    request_payload: web::Json<GetEnvVarRequestPayload>,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let name = &request_payload.name;

    // Cross-platform way to get environment variables.
    let value = env::var(name).ok();

    let response_data = GetEnvVarResponseData {
        value,
    };

    Ok(HttpResponse::Ok().json(response_data))
}
