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
    graph::{graphs_cache_find_by_id_mut, update_graph_in_property_json_file},
    pkg_info::belonging_pkg_info_find_by_graph_info,
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
    let pkgs_cache = state.pkgs_cache.read().await;
    let old_graphs_cache = state.graphs_cache.read().await.clone();

    // update graph info
    let graph_info = {
        let mut graphs_cache = state.graphs_cache.write().await;

        let graph_info =
            match graphs_cache_find_by_id_mut(&mut graphs_cache, &request_payload.graph_id) {
                Some(graph_info) => graph_info,
                None => {
                    let error_response = ErrorResponse {
                        status: Status::Fail,
                        message: format!("Graph with ID {} not found", request_payload.graph_id),
                        error: None,
                    };
                    return Ok(HttpResponse::BadRequest().json(error_response));
                }
            };

        // Update the auto_start field
        graph_info.auto_start = Some(request_payload.auto_start);

        graph_info.clone()
    };

    // update property.json file
    let new_graphs_cache = state.graphs_cache.read().await;
    if let Ok(Some(pkg_info)) = belonging_pkg_info_find_by_graph_info(&pkgs_cache, &graph_info) {
        if let (Some(app_base_dir), Some(property)) = (&graph_info.app_base_dir, &pkg_info.property)
        {
            if let Err(e) = update_graph_in_property_json_file(
                app_base_dir,
                property,
                &new_graphs_cache,
                &old_graphs_cache,
            ) {
                eprintln!("Warning: Failed to update property.json file: {e}");
            }
        }
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: UpdateGraphAutoStartResponseData {
            success: true,
        },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
