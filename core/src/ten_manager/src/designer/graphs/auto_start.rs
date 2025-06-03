//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::graphs_cache_find_by_id_mut,
};

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphAutoStartRequestPayload {
    pub graph_id: Uuid,
    pub auto_start: bool,
}

#[derive(Serialize, Deserialize, Debug, PartialEq)]
pub struct UpdateGraphAutoStartResponseData {
    pub success: bool,
}

pub async fn update_graph_auto_start_endpoint(
    request_payload: web::Json<UpdateGraphAutoStartRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut graphs_cache = state.graphs_cache.write().await;

    let graph_info = match graphs_cache_find_by_id_mut(
        &mut graphs_cache,
        &request_payload.graph_id,
    ) {
        Some(graph_info) => graph_info,
        None => {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: format!(
                    "Graph with ID {} not found",
                    request_payload.graph_id
                ),
                error: None,
            };
            return Ok(HttpResponse::BadRequest().json(error_response));
        }
    };

    // Update the auto_start field
    graph_info.auto_start = Some(request_payload.auto_start);

    let response = ApiResponse {
        status: Status::Ok,
        data: UpdateGraphAutoStartResponseData { success: true },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
