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
        connection::GraphConnection, connection::GraphDestination,
        connection::GraphLoc, connection::GraphMessageFlow, Graph,
    },
    pkg_info::message::MsgType,
};
use uuid::Uuid;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::{
        graphs_cache_find_by_id_mut,
        update_graph_connections_in_property_all_fields,
    },
    pkg_info::belonging_pkg_info_find_by_graph_info_mut,
};

#[derive(Serialize, Deserialize)]
pub struct DeleteGraphConnectionRequestPayload {
    pub graph_id: Uuid,

    pub src_app: Option<String>,
    pub src_extension: String,
    pub msg_type: MsgType,
    pub msg_name: String,
    pub dest_app: Option<String>,
    pub dest_extension: String,
}

#[derive(Serialize, Deserialize)]
pub struct DeleteGraphConnectionResponsePayload {
    pub success: bool,
}

fn graph_delete_connection(
    graph: &mut Graph,
    src_app: Option<String>,
    src_extension: String,
    msg_type: MsgType,
    msg_name: String,
    dest_app: Option<String>,
    dest_extension: String,
) -> Result<()> {
    // If no connections exist, return an error.
    if graph.connections.is_none() {
        return Err(anyhow!("No connections found in the graph"));
    }

    let connections = graph.connections.as_mut().unwrap();

    // Find the source node's connection in the connections list.
    let connection_idx = connections.iter().position(|conn| {
        conn.loc.app == src_app
            && (conn.loc.extension.as_ref() == Some(&src_extension))
    });

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
        // message flow.
        if let Some(flows) = message_flows {
            let flow_idx = flows.iter().position(|flow| {
                flow.name == msg_name
                    && flow.dest.iter().any(|dest| {
                        dest.loc.app == dest_app
                            && (dest.loc.extension.as_ref()
                                == Some(&dest_extension))
                    })
            });

            if let Some(flow_idx) = flow_idx {
                // If there are multiple destinations for this message, we
                // need to remove only the specific destination.
                let flow = &mut flows[flow_idx];

                // Find the destination to remove.
                let dest_idx = flow.dest.iter().position(|dest| {
                    dest.loc.app == dest_app
                        && (dest.loc.extension.as_ref()
                            == Some(&dest_extension))
                });

                if let Some(dest_idx) = dest_idx {
                    // Remove the specific destination.
                    flow.dest.remove(dest_idx);

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

                    return Ok(());
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

    // Delete the connection.
    if let Err(err) = graph_delete_connection(
        &mut graph_info.graph,
        request_payload.src_app.clone(),
        request_payload.src_extension.clone(),
        request_payload.msg_type.clone(),
        request_payload.msg_name.clone(),
        request_payload.dest_app.clone(),
        request_payload.dest_extension.clone(),
    ) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to delete connection: {err}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Ok(Some(pkg_info)) =
        belonging_pkg_info_find_by_graph_info_mut(&mut pkgs_cache, graph_info)
    {
        // Update property.json file to remove the connection.
        if let Some(property) = &mut pkg_info.property {
            // Create a GraphConnection representing what we
            // want to remove.
            let mut connection_to_remove = GraphConnection {
                loc: GraphLoc {
                    app: request_payload.src_app.clone(),
                    extension: Some(request_payload.src_extension.clone()),
                    subgraph: None,
                },
                cmd: None,
                data: None,
                audio_frame: None,
                video_frame: None,
            };

            // Create destination for the message flow.
            let dest_to_remove = GraphDestination {
                loc: GraphLoc {
                    app: request_payload.dest_app.clone(),
                    extension: Some(request_payload.dest_extension.clone()),
                    subgraph: None,
                },
                msg_conversion: None,
            };

            // Create the message flow with destination.
            let message_flow = GraphMessageFlow {
                name: request_payload.msg_name.clone(),
                dest: vec![dest_to_remove],
            };

            // Set the appropriate message type field.
            match request_payload.msg_type {
                MsgType::Cmd => {
                    connection_to_remove.cmd = Some(vec![message_flow]);
                }
                MsgType::Data => {
                    connection_to_remove.data = Some(vec![message_flow]);
                }
                MsgType::AudioFrame => {
                    connection_to_remove.audio_frame = Some(vec![message_flow]);
                }
                MsgType::VideoFrame => {
                    connection_to_remove.video_frame = Some(vec![message_flow]);
                }
            }

            let connections_to_remove = vec![connection_to_remove];

            // Write the updated property_all_fields map to
            // property.json.
            if let Err(e) = update_graph_connections_in_property_all_fields(
                &pkg_info.url,
                &mut property.all_fields,
                graph_info.name.as_ref().unwrap(),
                None,
                Some(&connections_to_remove),
                None,
            ) {
                eprintln!("Warning: Failed to update property.json file: {e}");
            }
        }
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: DeleteGraphConnectionResponsePayload { success: true },
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
            && conn
                .loc
                .extension
                .as_ref()
                .is_some_and(|ext| ext == src_extension)
    })
}

pub fn find_flow_with_name(
    flows: &[GraphMessageFlow],
    name: &str,
) -> Option<usize> {
    // Find flow with matching name.
    flows.iter().position(|flow| flow.name == name)
}

pub fn find_dest_with_extension(
    flow: &GraphMessageFlow,
    dest_app: &Option<String>,
    dest_extension: &str,
) -> Option<usize> {
    // Find destination with matching extension and app.
    flow.dest.iter().position(|dest| {
        dest.loc.app == *dest_app
            && dest
                .loc
                .extension
                .as_ref()
                .is_some_and(|ext| ext == dest_extension)
    })
}
