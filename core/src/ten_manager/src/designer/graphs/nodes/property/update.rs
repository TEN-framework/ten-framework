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
use ten_rust::graph::{graph_info::GraphInfo, node::GraphNode};
use uuid::Uuid;

use crate::{
    designer::{
        graphs::nodes::update_graph_node_in_property_json_file,
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::{graphs_cache_find_by_id_mut, nodes::validate::validate_extension_property},
};

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphNodePropertyRequestPayload {
    pub graph_id: Uuid,

    pub name: String,
    pub addon: String,
    pub extension_group: Option<String>,
    pub app: Option<String>,

    pub property: Option<serde_json::Value>,
}

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphNodePropertyResponsePayload {
    pub success: bool,
}

fn update_node_property_in_graph(
    graph_info: &mut GraphInfo,
    request_payload: &UpdateGraphNodePropertyRequestPayload,
) -> Result<()> {
    // Find the node in the graph.
    let graph_node = graph_info.graph.nodes_mut().iter_mut().find(|node| match node {
        GraphNode::Extension {
            content,
        } => {
            content.name == request_payload.name
                && content.addon == request_payload.addon
                && content.extension_group == request_payload.extension_group
                && content.app == request_payload.app
        }
        _ => false,
    });

    if graph_node.is_none() {
        return Err(anyhow::anyhow!(
            "Node '{}' with addon '{}' not found in graph '{}'",
            request_payload.name,
            request_payload.addon,
            request_payload.graph_id
        ));
    }

    // Update the node's property.
    match graph_node.unwrap() {
        GraphNode::Extension {
            content,
        } => {
            content.property = request_payload.property.clone();
        }
        _ => return Err(anyhow::anyhow!("Node is not an extension node")),
    }

    Ok(())
}

pub async fn update_graph_node_property_endpoint(
    request_payload: web::Json<UpdateGraphNodePropertyRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    // Get a write lock on the state since we need to modify the graph.
    let pkgs_cache = state.pkgs_cache.read().await;
    let mut graphs_cache = state.graphs_cache.write().await;
    let old_graphs_cache = graphs_cache.clone();

    // Get the specified graph from graphs_cache.
    let graph_info = match graphs_cache_find_by_id_mut(&mut graphs_cache, &request_payload.graph_id)
    {
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

    if let Err(e) = validate_extension_property(
        &request_payload.property,
        &request_payload.app,
        &request_payload.addon,
        &graph_info.app_base_dir,
        &pkgs_cache,
    ) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to validate extension property: {e}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Err(e) = update_node_property_in_graph(graph_info, &request_payload) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to update node property in graph: {e}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Err(e) = update_graph_node_in_property_json_file(
        &request_payload.graph_id,
        &pkgs_cache,
        &graphs_cache,
        &old_graphs_cache,
    ) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to update property.json file: {e}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: UpdateGraphNodePropertyResponsePayload {
            success: true,
        },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
