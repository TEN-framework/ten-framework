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

use super::update_graph_node_in_property_json_file;
use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::{
        graphs_cache_find_by_id_mut,
        nodes::{add::graph_add_extension_node, validate::validate_extension_property},
    },
};

#[derive(Serialize, Deserialize)]
pub struct AddGraphNodeRequestPayload {
    pub graph_id: Uuid,

    pub name: String,
    pub addon: String,
    pub app: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,

    pub property: Option<serde_json::Value>,
}

#[derive(Serialize, Deserialize)]
pub struct AddGraphNodeResponsePayload {
    pub success: bool,
}

pub async fn add_graph_node_endpoint(
    request_payload: web::Json<AddGraphNodeRequestPayload>,
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

    if let Err(e) = graph_add_extension_node(
        graph_info.graph.graph_mut(),
        &request_payload.name,
        &request_payload.addon,
        &request_payload.app,
        &request_payload.extension_group,
        &request_payload.property,
    )
    .await
    {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to add extension node: {e}"),
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
        data: AddGraphNodeResponsePayload {
            success: true,
        },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
