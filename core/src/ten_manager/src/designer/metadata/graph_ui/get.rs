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

use crate::config::metadata::GraphGeometry;
use crate::designer::response::{ApiResponse, Status};
use crate::designer::DesignerState;

#[derive(Debug, Serialize, Deserialize)]
pub struct GetGraphUiRequestPayload {
    pub graph_id: Uuid,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct GetGraphUiResponseData {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub graph_geometry: Option<GraphGeometry>,
}

pub async fn get_graph_ui_endpoint(
    request_payload: web::Json<GetGraphUiRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let graph_id = request_payload.graph_id;

    // Look for the graph geometry with the given graph_id.
    let graph_geometry = state
        .tman_metadata
        .read()
        .await
        .graph_ui
        .graphs_geometry
        .get(&graph_id)
        .cloned();

    let response_data = GetGraphUiResponseData { graph_geometry };

    let response =
        ApiResponse { status: Status::Ok, data: response_data, meta: None };

    Ok(HttpResponse::Ok().json(response))
}
