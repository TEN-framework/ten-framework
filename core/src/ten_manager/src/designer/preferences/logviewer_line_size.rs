//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use super::save_config_to_file;
use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Debug, Serialize, Deserialize)]
pub struct GetLogviewerLineSizeResponseData {
    pub logviewer_line_size: usize,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct UpdateLogviewerLineSizeRequestPayload {
    pub logviewer_line_size: usize,
}

/// Get the logviewer_line_size preference.
pub async fn get_logviewer_line_size_endpoint(
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let logviewer_line_size = state.tman_config.read().await.designer.logviewer_line_size;

    let response_data = GetLogviewerLineSizeResponseData {
        logviewer_line_size,
    };
    let response = ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}

/// Update the logviewer_line_size preference.
pub async fn update_logviewer_line_size_endpoint(
    request_payload: web::Json<UpdateLogviewerLineSizeRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    if request_payload.logviewer_line_size < 100 {
        return Err(actix_web::error::ErrorBadRequest(
            "logviewer_line_size must be greater than or equal to 100",
        ));
    }

    let mut tman_config = state.tman_config.write().await;

    // Update logviewer_line_size field.
    tman_config.designer.logviewer_line_size = request_payload.logviewer_line_size;

    // Save to config file.
    save_config_to_file(&mut tman_config)?;

    let response = ApiResponse {
        status: Status::Ok,
        data: (),
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
