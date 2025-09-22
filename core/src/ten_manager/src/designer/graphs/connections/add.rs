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
use ten_rust::{
    graph::{connection::GraphLoc, msg_conversion::MsgAndResultConversion},
    pkg_info::message::MsgType,
};
use uuid::Uuid;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    fs::json::patch_property_json_file,
    graph::{connections::add::graph_add_connection, graphs_cache_find_by_id_mut},
    pkg_info::belonging_pkg_info_find_by_graph_info_mut,
};

#[derive(Serialize, Deserialize)]
pub struct AddGraphConnectionRequestPayload {
    pub graph_id: Uuid,

    pub src: GraphLoc,
    pub dest: GraphLoc,

    pub msg_type: MsgType,
    pub msg_names: Vec<String>,

    pub msg_conversion: Option<MsgAndResultConversion>,
}

#[derive(Serialize, Deserialize)]
pub struct AddGraphConnectionResponsePayload {
    pub success: bool,
}

pub async fn add_graph_connection_endpoint(
    request_payload: web::Json<AddGraphConnectionRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let mut pkgs_cache = state.pkgs_cache.write().await;
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

    if let Err(e) = graph_add_connection(
        graph_info.graph.graph_mut(),
        &graph_info.app_base_dir,
        &pkgs_cache,
        request_payload.src.clone(),
        request_payload.dest.clone(),
        request_payload.msg_type.clone(),
        request_payload.msg_names.clone(),
        request_payload.msg_conversion.clone(),
    )
    .await
    {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to add connection: {e}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Ok(Some(pkg_info)) =
        belonging_pkg_info_find_by_graph_info_mut(&mut pkgs_cache, graph_info)
    {
        // Update property.json file with the updated graph.
        if let Some(property) = &mut pkg_info.property {
            // Update property.json file.
            if let Err(e) =
                patch_property_json_file(&pkg_info.url, property, &graphs_cache, &old_graphs_cache)
            {
                eprintln!("Warning: Failed to update property.json file: {e}");
            }
        }
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: AddGraphConnectionResponsePayload {
            success: true,
        },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
