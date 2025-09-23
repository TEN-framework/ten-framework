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
use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp,
    graph::{connection::GraphLoc, graph_info::GraphInfo, msg_conversion::MsgAndResultConversion},
    pkg_info::message::MsgType,
};
use uuid::Uuid;

use crate::{
    designer::{
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    fs::json::patch_property_json_file,
    graph::{
        connections::validate::{validate_connection_schema, MsgConversionValidateInfo},
        graphs_cache_find_by_id_mut,
    },
    pkg_info::belonging_pkg_info_find_by_graph_info,
};

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphConnectionMsgConversionRequestPayload {
    pub graph_id: Uuid,

    pub src: GraphLoc,
    pub dest: GraphLoc,
    pub msg_type: MsgType,
    pub msg_names: Vec<String>,

    pub msg_conversion: Option<MsgAndResultConversion>,
}

#[derive(Serialize, Deserialize)]
pub struct UpdateGraphConnectionMsgConversionResponsePayload {
    pub success: bool,
}

// Update the GraphInfo structure.
async fn update_graph_info(
    graph_info: &mut GraphInfo,
    request_payload: &web::Json<UpdateGraphConnectionMsgConversionRequestPayload>,
) -> Result<()> {
    // Store the original state in case validation fails.
    let original_graph = graph_info.graph.clone();

    // First check if connections exist in the graph.
    if let Some(connections) = &mut graph_info.graph.connections_mut() {
        // Try to find the matching connection based on app and extension.
        for connection in connections.iter_mut() {
            if request_payload.src.matches(&connection.loc) {
                // Find the correct message flow vector based on msg_type.
                let msg_flow_vec = match request_payload.msg_type {
                    MsgType::Cmd => &mut connection.cmd,
                    MsgType::Data => &mut connection.data,
                    MsgType::AudioFrame => &mut connection.audio_frame,
                    MsgType::VideoFrame => &mut connection.video_frame,
                };

                if request_payload.msg_names.len() > 1 {
                    // Restore the original graph
                    graph_info.graph = original_graph;
                    return Err(anyhow::anyhow!(
                        "Multiple message names are not supported for updating conversion"
                    ));
                }

                // If we found the message flow vector, find the specific
                // message flow by name.
                if let Some(msg_flows) = msg_flow_vec {
                    for msg_flow in msg_flows.iter_mut() {
                        if msg_flow.name.as_ref() == Some(&request_payload.msg_names[0]) {
                            // Find the matching destination
                            for dest in msg_flow.dest.iter_mut() {
                                if dest.loc.matches(&request_payload.dest) {
                                    // Update the msg_conversion field.
                                    dest.msg_conversion = request_payload.msg_conversion.clone();
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

    // Validate the updated graph.
    match graph_info.graph.validate_and_complete_and_flatten(None).await {
        Ok(_) => Ok(()),
        Err(e) => {
            // Restore the original graph if validation fails.
            graph_info.graph = original_graph;
            Err(e)
        }
    }
}

fn update_property_json_file(
    _request_payload: &web::Json<UpdateGraphConnectionMsgConversionRequestPayload>,
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    graphs_cache: &HashMap<Uuid, GraphInfo>,
    old_graphs_cache: &HashMap<Uuid, GraphInfo>,
) -> Result<()> {
    let graph_info = graphs_cache.get(&_request_payload.graph_id).unwrap();

    if let Ok(Some(pkg_info)) = belonging_pkg_info_find_by_graph_info(pkgs_cache, graph_info) {
        if let Some(property) = &pkg_info.property {
            patch_property_json_file(&pkg_info.url, property, graphs_cache, old_graphs_cache)?;
        }
    }
    Ok(())
}

pub async fn update_graph_connection_msg_conversion_endpoint(
    request_payload: web::Json<UpdateGraphConnectionMsgConversionRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
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

    // Validate connection schema first.
    if let Err(e) = validate_connection_schema(
        &pkgs_cache,
        graph_info.graph.graph_mut(),
        &graph_info.app_base_dir,
        &MsgConversionValidateInfo {
            src: &request_payload.src,
            dest: &request_payload.dest,
            msg_type: &request_payload.msg_type,
            msg_names: &request_payload.msg_names,
            msg_conversion: &request_payload.msg_conversion,
        },
    )
    .await
    {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to validate connection schema: {e}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Err(e) = update_graph_info(graph_info, &request_payload).await {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to update graph info: {e}"),
            error: None,
        };
        return Ok(HttpResponse::BadRequest().json(error_response));
    }

    if let Err(e) =
        update_property_json_file(&request_payload, &pkgs_cache, &graphs_cache, &old_graphs_cache)
    {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Failed to update property.json file: {e}"),
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
