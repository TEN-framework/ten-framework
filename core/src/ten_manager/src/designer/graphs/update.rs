//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use ten_rust::graph::node::GraphNode;
use ten_rust::graph::{connection::GraphConnection, GraphExposedMessage};
use uuid::Uuid;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::{
        graphs_cache_find_by_id_mut, replace_graph_nodes_and_connections,
        update_graph_all_fields,
    },
    pkg_info::belonging_pkg_info_find_by_graph_info_mut,
};

#[derive(Serialize, Deserialize)]
pub struct GraphNodeForUpdate {
    pub name: String,
    pub addon: String,
    pub extension_group: Option<String>,
    pub app: Option<String>,
    pub property: Option<serde_json::Value>,
}

impl GraphNodeForUpdate {
    fn to_graph_node(&self) -> GraphNode {
        GraphNode::new_extension_node(
            self.name.clone(),
            self.addon.clone(),
            self.extension_group.clone(),
            self.app.clone(),
            self.property.clone(),
        )
    }
}

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphRequestPayload {
    pub graph_id: Uuid,
    pub nodes: Vec<GraphNodeForUpdate>,
    pub connections: Vec<GraphConnection>,

    #[serde(default)]
    pub exposed_messages: Vec<GraphExposedMessage>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub auto_start: Option<bool>,
}

#[derive(Serialize, Deserialize, Debug, PartialEq)]
pub struct UpdateGraphResponseData {
    pub success: bool,
}

pub async fn update_graph_endpoint(
    request_payload: web::Json<UpdateGraphRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut pkgs_cache = state.pkgs_cache.write().await;
    let mut graphs_cache = state.graphs_cache.write().await;

    // Convert GraphNodeForUpdate to GraphNode
    let graph_nodes: Vec<GraphNode> = request_payload
        .nodes
        .iter()
        .map(|node_update| node_update.to_graph_node())
        .collect();

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

    let pkg_info = match belonging_pkg_info_find_by_graph_info_mut(
        &mut pkgs_cache,
        graph_info,
    ) {
        Ok(Some(pkg_info)) => pkg_info,
        Ok(None) => {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: "App package not found".to_string(),
                error: None,
            };
            return Ok(HttpResponse::BadRequest().json(error_response));
        }
        Err(err) => {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: err.to_string(),
                error: None,
            };
            return Ok(HttpResponse::BadRequest().json(error_response));
        }
    };

    // Access the graph and update it.
    match replace_graph_nodes_and_connections(
        graph_info.graph.graph_mut(),
        &graph_nodes,
        &request_payload.connections,
        &request_payload.exposed_messages,
        &[],
    ) {
        Ok(_) => (),
        Err(err) => {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: err.to_string(),
                error: None,
            };
            return Ok(HttpResponse::BadRequest().json(error_response));
        }
    }

    assert!(graph_info.app_base_dir.is_some());
    assert!(pkg_info.property.is_some());

    match update_graph_all_fields(
        graph_info.app_base_dir.as_ref().unwrap(),
        &mut pkg_info.property.as_mut().unwrap().all_fields,
        graph_info.name.as_ref().unwrap(),
        &graph_nodes,
        &request_payload.connections,
        &request_payload.exposed_messages,
        &[],
        request_payload.auto_start,
    ) {
        Ok(_) => (),
        Err(err) => {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: err.to_string(),
                error: None,
            };
            return Ok(HttpResponse::BadRequest().json(error_response));
        }
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: UpdateGraphResponseData { success: true },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
