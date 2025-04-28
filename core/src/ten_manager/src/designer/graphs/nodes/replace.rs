//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use anyhow::Result;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::designer::graphs::nodes::{
    update_graph_node_in_property_all_fields, GraphNodeUpdateAction,
};
use crate::designer::{
    response::{ApiResponse, ErrorResponse, Status},
    DesignerState,
};
use crate::graph::{
    graphs_cache_find_by_id_mut, nodes::validate::validate_extension_property,
};

#[derive(Serialize, Deserialize)]
pub struct ReplaceGraphNodeRequestPayload {
    pub graph_id: Uuid,

    pub app: Option<String>,
    pub name: String,
    pub addon: String,
    pub property: Option<serde_json::Value>,
}

#[derive(Serialize, Deserialize)]
pub struct ReplaceGraphNodeResponsePayload {
    pub success: bool,
}

pub async fn replace_graph_node_endpoint(
    request_payload: web::Json<ReplaceGraphNodeRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    // Get a write lock on the state since we need to modify the graph.
    let mut pkgs_cache = state.pkgs_cache.write().await;
    let mut graphs_cache = state.graphs_cache.write().await;

    // Get the specified graph from graphs_cache.
    let graph_info = match graphs_cache_find_by_id_mut(
        &mut graphs_cache,
        &request_payload.graph_id,
    ) {
        Some(graph_info) => graph_info,
        None => {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: "Graph not found".to_string(),
                error: None,
            };
            return Ok(HttpResponse::NotFound().json(error_response));
        }
    };

    // Check if property conforms to schema.
    if let Err(e) = validate_extension_property(
        &request_payload.property,
        &request_payload.app,
        &request_payload.addon,
        &graph_info.app_base_dir,
        &pkgs_cache,
    ) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to validate extension property: {}", e),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    // Find the graph node in the graph.
    let graph_node = graph_info.graph.nodes.iter_mut().find(|node| {
        node.type_and_name.name == request_payload.name
            && node.app == request_payload.app
    });

    if graph_node.is_none() {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!(
                "Node '{}' with app '{:?}' not found in graph '{}'",
                request_payload.name,
                request_payload.app,
                request_payload.graph_id
            ),
            error: None,
        };
        return Ok(HttpResponse::NotFound().json(error_response));
    }

    // Replace the addon and property of the graph node.
    let graph_node = graph_node.unwrap();
    let extension_group = graph_node.extension_group.clone();
    graph_node.addon = request_payload.addon.clone();
    graph_node.property = request_payload.property.clone();

    // Update property.json file with the updated graph node.
    if let Err(e) = update_graph_node_in_property_all_fields(
        &mut pkgs_cache,
        graph_info,
        &request_payload.name,
        &request_payload.addon,
        &extension_group,
        &request_payload.app,
        &request_payload.property,
        GraphNodeUpdateAction::Update,
    ) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to update property.json file: {}", e),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: ReplaceGraphNodeResponsePayload { success: true },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
