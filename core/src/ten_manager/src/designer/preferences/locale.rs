//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use super::{super::locale::Locale, save_config_to_file};
use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Debug, Serialize, Deserialize)]
pub struct GetLocaleResponseData {
    pub locale: Locale,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct UpdateLocaleRequestPayload {
    pub locale: Locale,
}

/// Get the locale preference.
pub async fn get_locale_endpoint(
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let locale = state.tman_config.read().await.designer.locale;

    let response_data = GetLocaleResponseData {
        locale,
    };
    let response = ApiResponse {
        status: Status::Ok,
        data: response_data,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}

/// Update the locale preference.
pub async fn update_locale_endpoint(
    request_payload: web::Json<UpdateLocaleRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut tman_config = state.tman_config.write().await;

    // Update locale field.
    tman_config.designer.locale = request_payload.locale;

    // Save to config file.
    save_config_to_file(&mut tman_config)?;

    let response = ApiResponse {
        status: Status::Ok,
        data: (),
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
