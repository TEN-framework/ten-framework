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
    graph::{graphs_cache_find_by_id_mut, update_graph_all_fields},
    pkg_info::belonging_pkg_info_find_by_graph_info_mut,
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
    let mut pkgs_cache = state.pkgs_cache.write().await;
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

    // Try to update property.json file with the auto_start change
    if let Ok(Some(pkg_info)) =
        belonging_pkg_info_find_by_graph_info_mut(&mut pkgs_cache, graph_info)
    {
        if let (Some(app_base_dir), Some(property), Some(graph_name)) =
            (&graph_info.app_base_dir, &mut pkg_info.property, &graph_info.name)
        {
            if let Err(e) = update_graph_all_fields(
                app_base_dir,
                &mut property.all_fields,
                graph_name,
                graph_info.graph.nodes(),
                graph_info.graph.connections().as_ref().unwrap_or(&vec![]),
                graph_info.graph.exposed_messages().as_ref().unwrap_or(&vec![]),
                graph_info
                    .graph
                    .exposed_properties()
                    .as_ref()
                    .unwrap_or(&vec![]),
                Some(request_payload.auto_start),
            ) {
                eprintln!("Warning: Failed to update property.json file: {e}");
            }
        }
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: UpdateGraphAutoStartResponseData { success: true },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
