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

use ten_rust::graph::graph_info::GraphInfo;

use crate::{
    designer::{
        graphs::nodes::{
            update_graph_node_in_property_all_fields, GraphNodeUpdateAction,
        },
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::{
        graphs_cache_find_by_id_mut,
        nodes::validate::validate_extension_property,
    },
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
    let graph_node = graph_info.graph.nodes.iter_mut().find(|node| {
        node.type_and_name.name == request_payload.name
            && node.addon == request_payload.addon
            && node.extension_group == request_payload.extension_group
            && node.app == request_payload.app
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
    graph_node.unwrap().property = request_payload.property.clone();

    Ok(())
}

pub async fn update_graph_node_property_endpoint(
    request_payload: web::Json<UpdateGraphNodePropertyRequestPayload>,
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

    if let Err(e) = update_node_property_in_graph(graph_info, &request_payload)
    {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to update node property in graph: {}", e),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Err(e) = update_graph_node_in_property_all_fields(
        &mut pkgs_cache,
        graph_info,
        &request_payload.name,
        &request_payload.addon,
        &request_payload.extension_group,
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
        data: UpdateGraphNodePropertyResponsePayload { success: true },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
