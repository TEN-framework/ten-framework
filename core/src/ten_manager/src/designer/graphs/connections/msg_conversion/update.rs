//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{web, HttpResponse, Responder};
use anyhow::Result;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp,
    graph::{
        connection::{GraphConnection, GraphDestination, GraphMessageFlow},
        graph_info::GraphInfo,
        msg_conversion::MsgAndResultConversion,
    },
    pkg_info::message::MsgType,
};

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::{
        connections::validate::{
            validate_connection_schema, MsgConversionValidateInfo,
        },
        graphs_cache_find_by_id_mut, update_graph_connections_all_fields,
    },
    pkg_info::belonging_pkg_info_find_by_graph_info_mut,
};

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphConnectionMsgConversionRequestPayload {
    pub graph_id: Uuid,

    pub src_app: Option<String>,
    pub src_extension: String,
    pub msg_type: MsgType,
    pub msg_name: String,
    pub dest_app: Option<String>,
    pub dest_extension: String,

    pub msg_conversion: Option<MsgAndResultConversion>,
}

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphConnectionMsgConversionResponsePayload {
    pub success: bool,
}

// Update the GraphInfo structure.
fn update_graph_info(
    graph_info: &mut GraphInfo,
    request_payload: &web::Json<
        UpdateGraphConnectionMsgConversionRequestPayload,
    >,
) -> Result<()> {
    // First check if connections exist in the graph.
    if let Some(connections) = &mut graph_info.graph.connections {
        // Try to find the matching connection based on app and extension.
        for connection in connections.iter_mut() {
            if connection.app == request_payload.src_app
                && connection.extension == request_payload.src_extension
            {
                // Find the correct message flow vector based on msg_type.
                let msg_flow_vec = match request_payload.msg_type {
                    MsgType::Cmd => &mut connection.cmd,
                    MsgType::Data => &mut connection.data,
                    MsgType::AudioFrame => &mut connection.audio_frame,
                    MsgType::VideoFrame => &mut connection.video_frame,
                };

                // If we found the message flow vector, find the specific
                // message flow by name.
                if let Some(msg_flows) = msg_flow_vec {
                    for msg_flow in msg_flows.iter_mut() {
                        if msg_flow.name == request_payload.msg_name {
                            // Find the matching destination
                            for dest in msg_flow.dest.iter_mut() {
                                if dest.app == request_payload.dest_app
                                    && dest.extension
                                        == request_payload.dest_extension
                                {
                                    // Update the msg_conversion field.
                                    dest.msg_conversion =
                                        request_payload.msg_conversion.clone();
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }
        }
    }

    Ok(())
}

fn update_property_all_fields(
    graph_info: &mut GraphInfo,
    request_payload: &web::Json<
        UpdateGraphConnectionMsgConversionRequestPayload,
    >,
    pkgs_cache: &mut HashMap<String, PkgsInfoInApp>,
) -> Result<()> {
    if let Ok(Some(pkg_info)) =
        belonging_pkg_info_find_by_graph_info_mut(pkgs_cache, graph_info)
    {
        // Check if the property exists.
        if let Some(property) = &mut pkg_info.property {
            // Create a GraphConnection with the message conversion to
            // update.
            let mut connection = GraphConnection {
                app: request_payload.src_app.clone(),
                extension: request_payload.src_extension.clone(),
                cmd: None,
                data: None,
                audio_frame: None,
                video_frame: None,
            };

            // Create the destination.
            let destination = GraphDestination {
                app: request_payload.dest_app.clone(),
                extension: request_payload.dest_extension.clone(),
                msg_conversion: request_payload.msg_conversion.clone(),
            };

            // Create the message flow.
            let message_flow = GraphMessageFlow {
                name: request_payload.msg_name.clone(),
                dest: vec![destination],
            };

            // Set the appropriate message type field.
            match request_payload.msg_type {
                MsgType::Cmd => {
                    connection.cmd = Some(vec![message_flow]);
                }
                MsgType::Data => {
                    connection.data = Some(vec![message_flow]);
                }
                MsgType::AudioFrame => {
                    connection.audio_frame = Some(vec![message_flow]);
                }
                MsgType::VideoFrame => {
                    connection.video_frame = Some(vec![message_flow]);
                }
            }

            // Update the connection with the new message conversion.
            let connections_to_modify = vec![connection];

            // Update the property.json file.
            if let Err(e) = update_graph_connections_all_fields(
                &pkg_info.url,
                &mut property.all_fields,
                graph_info.name.as_ref().unwrap(),
                None,
                None,
                Some(&connections_to_modify),
            ) {
                // Return error if failed to update.
                return Err(anyhow::anyhow!(
                    "Failed to update message conversion: {}",
                    e
                ));
            }
        }
    }

    Ok(())
}

pub async fn update_graph_connection_msg_conversion_endpoint(
    request_payload: web::Json<
        UpdateGraphConnectionMsgConversionRequestPayload,
    >,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
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

    // Validate connection schema first.
    if let Err(e) = validate_connection_schema(
        &pkgs_cache,
        &mut graph_info.graph,
        &graph_info.app_base_dir,
        &MsgConversionValidateInfo {
            src_app: &request_payload.src_app,
            src_extension: &request_payload.src_extension,
            msg_type: &request_payload.msg_type,
            msg_name: &request_payload.msg_name,
            dest_app: &request_payload.dest_app,
            dest_extension: &request_payload.dest_extension,
            msg_conversion: &request_payload.msg_conversion,
        },
    ) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to validate connection schema: {}", e),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Err(e) = update_graph_info(graph_info, &request_payload) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to update graph info: {}", e),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Err(e) = update_property_all_fields(
        graph_info,
        &request_payload,
        &mut pkgs_cache,
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
        data: UpdateGraphConnectionMsgConversionResponsePayload {
            success: true,
        },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
