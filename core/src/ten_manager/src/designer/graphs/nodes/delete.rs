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
use ten_rust::graph::{node::GraphNode, Graph};
use uuid::Uuid;

use super::update_graph_node_in_property_json_file;
use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::graphs_cache_find_by_id_mut,
};

#[derive(Serialize, Deserialize)]
pub struct DeleteGraphNodeRequestPayload {
    pub graph_id: Uuid,

    pub name: String,
    pub addon: String,
    pub app: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,
}

#[derive(Serialize, Deserialize)]
pub struct DeleteGraphNodeResponsePayload {
    pub success: bool,
}

pub async fn graph_delete_extension_node(
    graph: &mut Graph,
    pkg_name: String,
    addon: String,
    app: Option<String>,
    extension_group: Option<String>,
) -> Result<()> {
    // Store the original state in case validation fails.
    let original_graph = graph.clone();

    // Find and remove the matching node.
    let original_nodes_len = graph.nodes.len();
    graph.nodes.retain(|node| {
        let extension_node = match node {
            GraphNode::Extension {
                content,
            } => content,
            _ => return true, // Keep other node types.
        };

        !(extension_node.name == pkg_name
            && extension_node.addon == addon
            && extension_node.app == app
            && extension_node.extension_group == extension_group)
    });

    // If no node was removed, return early.
    if graph.nodes.len() == original_nodes_len {
        return Ok(());
    }

    // The node was removed, now clean up any connections.
    if let Some(connections) = &mut graph.connections {
        // 1. Remove entire connections with matching app and extension.
        connections.retain(|conn| {
            !((conn.loc.extension.as_ref() == Some(&pkg_name)) && conn.loc.app == app)
        });

        // 2. Remove destinations from message flows in all connections.
        for connection in connections.iter_mut() {
            // Process cmd flows.
            if let Some(cmd_flows) = &mut connection.cmd {
                for flow in cmd_flows.iter_mut() {
                    flow.dest.retain(|dest| {
                        !((dest.loc.extension.as_ref() == Some(&pkg_name)) && dest.loc.app == app)
                    });
                }
                // Remove empty cmd flows.
                cmd_flows.retain(|flow| !flow.dest.is_empty());
            }

            // Process data flows.
            if let Some(data_flows) = &mut connection.data {
                for flow in data_flows.iter_mut() {
                    flow.dest.retain(|dest| {
                        !((dest.loc.extension.as_ref() == Some(&pkg_name)) && dest.loc.app == app)
                    });
                }
                // Remove empty data flows.
                data_flows.retain(|flow| !flow.dest.is_empty());
            }

            // Process audio_frame flows.
            if let Some(audio_flows) = &mut connection.audio_frame {
                for flow in audio_flows.iter_mut() {
                    flow.dest.retain(|dest| {
                        !((dest.loc.extension.as_ref() == Some(&pkg_name)) && dest.loc.app == app)
                    });
                }
                // Remove empty audio_frame flows.
                audio_flows.retain(|flow| !flow.dest.is_empty());
            }

            // Process video_frame flows.
            if let Some(video_flows) = &mut connection.video_frame {
                for flow in video_flows.iter_mut() {
                    flow.dest.retain(|dest| {
                        !((dest.loc.extension.as_ref() == Some(&pkg_name)) && dest.loc.app == app)
                    });
                }
                // Remove empty video_frame flows.
                video_flows.retain(|flow| !flow.dest.is_empty());
            }
        }

        // Remove connections that have no message flows left.
        connections.retain(|conn| {
            let has_cmd = conn.cmd.as_ref().is_some_and(|c| !c.is_empty());
            let has_data = conn.data.as_ref().is_some_and(|d| !d.is_empty());
            let has_audio = conn.audio_frame.as_ref().is_some_and(|a| !a.is_empty());
            let has_video = conn.video_frame.as_ref().is_some_and(|v| !v.is_empty());
            has_cmd || has_data || has_audio || has_video
        });

        // If no connections left, set connections to None.
        if connections.is_empty() {
            graph.connections = None;
        }
    }

    // Validate the graph.
    match graph.validate_and_complete(None) {
        Ok(_) => Ok(()),
        Err(e) => {
            // Restore the original graph if validation fails.
            *graph = original_graph;
            Err(e)
        }
    }
}

pub async fn delete_graph_node_endpoint(
    request_payload: web::Json<DeleteGraphNodeRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    // Get a write lock on the state since we need to modify the graph.
    let pkgs_cache = state.pkgs_cache.write().await;
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

    // Delete the extension node.
    if let Err(err) = graph_delete_extension_node(
        graph_info.graph.graph_mut(),
        request_payload.name.clone(),
        request_payload.addon.clone(),
        request_payload.app.clone(),
        request_payload.extension_group.clone(),
    )
    .await
    {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to delete node: {err}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    // Try to update property.json file if possible.
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

    // Return success response
    let response = ApiResponse {
        status: Status::Ok,
        data: DeleteGraphNodeResponsePayload {
            success: true,
        },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
