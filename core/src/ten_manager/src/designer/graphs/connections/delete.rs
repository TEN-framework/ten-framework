//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use ten_rust::{
    graph::{
        connection::{GraphConnection, GraphLoc, GraphMessageFlow},
        Graph,
    },
    pkg_info::message::MsgType,
};
use uuid::Uuid;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    fs::json::patch_property_json_file,
    graph::graphs_cache_find_by_id_mut,
    pkg_info::belonging_pkg_info_find_by_graph_info,
};

#[derive(Serialize, Deserialize)]
pub struct DeleteGraphConnectionRequestPayload {
    pub graph_id: Uuid,

    pub src: GraphLoc,
    pub dest: GraphLoc,

    pub msg_type: MsgType,
    pub msg_names: Vec<String>,
}

#[derive(Serialize, Deserialize)]
pub struct DeleteGraphConnectionResponsePayload {
    pub success: bool,
}

async fn graph_delete_connection(
    graph: &mut Graph,
    src: GraphLoc,
    dest: GraphLoc,
    msg_type: MsgType,
    msg_names: Vec<String>,
) -> Result<()> {
    // Store the original state in case validation fails.
    let original_graph = graph.clone();

    // If no connections exist, return an error.
    if graph.connections.is_none() {
        return Err(anyhow!("No connections found in the graph"));
    }

    let connections = graph.connections.as_mut().unwrap();

    // Find the source node's connection in the connections list.
    let connection_idx = connections.iter().position(|conn| conn.loc.matches(&src));

    if let Some(idx) = connection_idx {
        let connection = &mut connections[idx];

        // Determine which message type array we need to modify.
        let message_flows = match msg_type {
            MsgType::Cmd => &mut connection.cmd,
            MsgType::Data => &mut connection.data,
            MsgType::AudioFrame => &mut connection.audio_frame,
            MsgType::VideoFrame => &mut connection.video_frame,
        };

        // If the message flows array exists, find and remove the specific
        // message flows.
        if let Some(flows) = message_flows {
            for (flow_idx, flow) in flows.iter_mut().enumerate() {
                // Find the destination to remove.
                let dest_idx = flow.dest.iter().position(|dest_iter| dest_iter.loc.matches(&dest));

                if let Some(dest_idx) = dest_idx {
                    // Since 'name' and 'names' in GraphMessageFlow are mutually exclusive,
                    // we need to check both fields.
                    // Deletion will happen only when the name/names set in the payload
                    // matches the flow exactly.

                    let mut deletion_complete = false;

                    // Check the 'name' field.
                    if msg_names.len() == 1 && Some(&msg_names[0]) == flow.name.as_ref() {
                        flow.dest.remove(dest_idx);
                        deletion_complete = true;
                    }
                    // Check the 'names' field.
                    else if let Some(names) = flow.names.as_ref() {
                        if msg_names.len() == names.len()
                            && msg_names.iter().all(|name| names.contains(name))
                        {
                            flow.dest.remove(dest_idx);
                            deletion_complete = true;
                        }
                    }

                    // When deletion is completed, check if the whole flow/flows/connection
                    // needs to be removed, and then validate before return
                    if deletion_complete {
                        // If there are no more destinations, remove the whole
                        // flow.
                        if flow.dest.is_empty() {
                            flows.remove(flow_idx);
                        }

                        // If there are no more flows of this message type, set
                        // the array to None.
                        if flows.is_empty() {
                            *message_flows = None;
                        }

                        // If there are no message flows left in this
                        // connection, remove the connection.
                        if connection.cmd.is_none()
                            && connection.data.is_none()
                            && connection.audio_frame.is_none()
                            && connection.video_frame.is_none()
                        {
                            connections.remove(idx);
                        }

                        // If there are no more connections, set the connections
                        // field to None.
                        if connections.is_empty() {
                            graph.connections = None;
                        }

                        // Validate the updated graph.
                        match graph.validate_and_complete(None) {
                            Ok(_) => return Ok(()),
                            Err(e) => {
                                // Restore the original graph if validation fails.
                                *graph = original_graph;
                                return Err(e);
                            }
                        }
                    }
                }
            }
        }
    }
    // Connection, message flow, or destination not found.
    Err(anyhow!("Connection not found in the graph"))
}

pub async fn delete_graph_connection_endpoint(
    request_payload: web::Json<DeleteGraphConnectionRequestPayload>,
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

    // Delete the connection.
    if let Err(err) = graph_delete_connection(
        graph_info.graph.graph_mut(),
        request_payload.src.clone(),
        request_payload.dest.clone(),
        request_payload.msg_type.clone(),
        request_payload.msg_names.clone(),
    )
    .await
    {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to delete connection: {err}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Ok(Some(pkg_info)) = belonging_pkg_info_find_by_graph_info(&pkgs_cache, graph_info) {
        // Update property.json file to remove the connection.
        if let Some(property) = &pkg_info.property {
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
        data: DeleteGraphConnectionResponsePayload {
            success: true,
        },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}

pub fn find_connection_with_extensions<'a>(
    connection_list: &'a [GraphConnection],
    src_app: &Option<String>,
    src_extension: &str,
) -> Option<&'a GraphConnection> {
    // Find connection with matching src app and extension.
    connection_list.iter().find(|conn| {
        conn.loc.app == *src_app
            && conn.loc.extension.as_ref().is_some_and(|ext| ext == src_extension)
    })
}

pub fn find_flow_with_name(flows: &[GraphMessageFlow], name: &str) -> Option<usize> {
    // Find flow with matching name.
    flows.iter().position(|flow| flow.name.as_deref() == Some(name))
}

pub fn find_dest_with_extension(
    flow: &GraphMessageFlow,
    dest_app: &Option<String>,
    dest_extension: &str,
) -> Option<usize> {
    // Find destination with matching extension and app.
    flow.dest.iter().position(|dest| {
        dest.loc.app == *dest_app
            && dest.loc.extension.as_ref().is_some_and(|ext| ext == dest_extension)
    })
}
