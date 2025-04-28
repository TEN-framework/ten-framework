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

use ten_rust::{
    graph::node::GraphNode,
    pkg_info::{
        find_pkgs_cache_entry_by_app_uri, get_pkg_info_for_extension_addon,
        message::{MsgDirection, MsgType},
    },
};

use crate::{
    designer::{
        graphs::nodes::get_extension_nodes_in_graph,
        response::{ApiResponse, ErrorResponse, Status},
        DesignerState,
    },
    graph::compatible::{
        get_compatible_msg_extension, CompatibleExtensionAndMsg,
    },
};

/// Represents the request payload for retrieving compatible messages.
///
/// This struct contains all necessary information to identify a specific
/// message within a graph and find other messages that are compatible with it.
#[derive(Debug, Deserialize, Serialize)]
pub struct GetCompatibleMsgsRequestPayload {
    /// ID of the graph to search for compatible messages.
    pub graph_id: Uuid,

    /// Optional application name that contains the extension.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app: Option<String>,

    /// Optional extension group that the extension belongs to.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,

    /// Name of the extension to find compatible messages for.
    pub extension: String,

    /// Type of the message (e.g., "command", "data").
    pub msg_type: MsgType,

    /// Direction of the message (e.g., "input", "output").
    pub msg_direction: MsgDirection,

    /// Name of the specific message to find compatibility with.
    pub msg_name: String,
}

#[derive(Debug, Deserialize, Serialize, PartialEq)]
pub struct GetCompatibleMsgsSingleResponseData {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,

    pub extension: String,
    pub msg_type: MsgType,
    pub msg_direction: MsgDirection,
    pub msg_name: String,
}

impl From<CompatibleExtensionAndMsg<'_>>
    for GetCompatibleMsgsSingleResponseData
{
    fn from(compatible: CompatibleExtensionAndMsg) -> Self {
        GetCompatibleMsgsSingleResponseData {
            app: compatible.extension.app.clone(),
            extension_group: compatible.extension.extension_group.clone(),
            extension: compatible.extension.type_and_name.name.clone(),
            msg_type: compatible.msg_type,
            msg_direction: compatible.msg_direction,
            msg_name: compatible.msg_name,
        }
    }
}

fn get_extension_graph_node<'a>(
    extension_graph_nodes: &'a [GraphNode],
    app: &Option<String>,
    extension_group: &Option<String>,
    extension_name: &str,
) -> Result<&'a GraphNode> {
    for extension in extension_graph_nodes {
        if extension.type_and_name.name == extension_name
            && extension.extension_group.as_ref() == extension_group.as_ref()
            && extension.app == *app
        {
            return Ok(extension);
        }
    }

    Err(anyhow::anyhow!(
        "Failed to find extension '{}' in extension group '{:?}', app: '{:?}'",
        extension_name,
        extension_group,
        app
    ))
}

pub async fn get_compatible_messages_endpoint(
    request_payload: web::Json<GetCompatibleMsgsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let pkgs_cache = state.pkgs_cache.read().await;
    let graphs_cache = state.graphs_cache.read().await;

    let graph_info = graphs_cache.get(&request_payload.graph_id);
    let app_base_dir_of_graph = match graph_info {
        Some(graph_info) => &graph_info.app_base_dir,
        None => &None,
    };

    match find_pkgs_cache_entry_by_app_uri(&pkgs_cache, &request_payload.app) {
        Some((_, pkgs_info_in_app)) => {
            if pkgs_info_in_app.app_pkg_info.is_none() {
                let error_response = ErrorResponse {
                    status: Status::Fail,
                    message: "Application package information is missing"
                        .to_string(),
                    error: None,
                };
                return Ok(HttpResponse::NotFound().json(error_response));
            }

            let extension_graph_nodes = match get_extension_nodes_in_graph(
                &request_payload.graph_id,
                &graphs_cache,
            ) {
                Ok(exts) => exts,
                Err(err) => {
                    let error_response = ErrorResponse::from_error(
                        &err,
                        format!(
                            "Error fetching runtime extensions for graph '{}'",
                            request_payload.graph_id
                        )
                        .as_str(),
                    );
                    return Ok(HttpResponse::NotFound().json(error_response));
                }
            };

            let extension_graph_node = match get_extension_graph_node(
                extension_graph_nodes,
                &request_payload.app,
                &request_payload.extension_group,
                &request_payload.extension,
            ) {
                Ok(ext) => ext,
                Err(err) => {
                    let error_response = ErrorResponse::from_error(
                        &err,
                        format!(
                            "Failed to find the extension: {}",
                            request_payload.extension
                        )
                        .as_str(),
                    );

                    return Ok(HttpResponse::NotFound().json(error_response));
                }
            };

            let msg_ty = request_payload.msg_type.clone();
            let msg_dir = request_payload.msg_direction.clone();

            let mut desired_msg_dir = msg_dir.clone();
            desired_msg_dir.toggle();

            if let Some(extension_pkg_info) = get_pkg_info_for_extension_addon(
                &pkgs_cache,
                app_base_dir_of_graph,
                &extension_graph_node.app,
                &extension_graph_node.addon,
            ) {
                let compatible_list = match msg_ty {
                    MsgType::Cmd => {
                        let src_cmd_schema = extension_pkg_info
                            .schema_store
                            .as_ref()
                            .and_then(|schema_store| match msg_dir {
                                MsgDirection::In => schema_store
                                    .cmd_in
                                    .get(request_payload.msg_name.as_str()),
                                MsgDirection::Out => schema_store
                                    .cmd_out
                                    .get(request_payload.msg_name.as_str()),
                            });

                        match get_compatible_msg_extension(
                            extension_graph_nodes,
                            app_base_dir_of_graph,
                            &pkgs_cache,
                            &desired_msg_dir,
                            src_cmd_schema,
                            &msg_ty,
                            request_payload.msg_name.clone(),
                        ) {
                            Ok(results) => results,
                            Err(err) => {
                                let error_response = ErrorResponse::from_error(
                                    &err,
                                    format!(
                                        "Failed to find compatible cmd/{}:",
                                        request_payload.msg_name
                                    )
                                    .as_str(),
                                );
                                return Ok(HttpResponse::NotFound()
                                    .json(error_response));
                            }
                        }
                    }
                    _ => {
                        let src_msg_schema = extension_pkg_info
                            .schema_store
                            .as_ref()
                            .and_then(|schema_store| match msg_ty {
                                MsgType::Data => match msg_dir {
                                    MsgDirection::In => schema_store
                                        .data_in
                                        .get(request_payload.msg_name.as_str()),
                                    MsgDirection::Out => schema_store
                                        .data_out
                                        .get(request_payload.msg_name.as_str()),
                                },
                                MsgType::AudioFrame => match msg_dir {
                                    MsgDirection::In => schema_store
                                        .audio_frame_in
                                        .get(request_payload.msg_name.as_str()),
                                    MsgDirection::Out => schema_store
                                        .audio_frame_out
                                        .get(request_payload.msg_name.as_str()),
                                },
                                MsgType::VideoFrame => match msg_dir {
                                    MsgDirection::In => schema_store
                                        .video_frame_in
                                        .get(request_payload.msg_name.as_str()),
                                    MsgDirection::Out => schema_store
                                        .video_frame_out
                                        .get(request_payload.msg_name.as_str()),
                                },
                                _ => panic!("should not happen."),
                            });

                        match get_compatible_msg_extension(
                            extension_graph_nodes,
                            app_base_dir_of_graph,
                            &pkgs_cache,
                            &desired_msg_dir,
                            src_msg_schema,
                            &msg_ty,
                            request_payload.msg_name.clone(),
                        ) {
                            Ok(results) => results,
                            Err(err) => {
                                let error_response = ErrorResponse::from_error(
                                    &err,
                                    format!(
                                        "Failed to find compatible {}:",
                                        request_payload.msg_name
                                    )
                                    .as_str(),
                                );
                                return Ok(HttpResponse::NotFound()
                                    .json(error_response));
                            }
                        }
                    }
                };

                let response_data: Vec<GetCompatibleMsgsSingleResponseData> =
                    compatible_list.into_iter().map(Into::into).collect();

                let response = ApiResponse {
                    status: Status::Ok,
                    data: response_data,
                    meta: None,
                };

                Ok(HttpResponse::Ok().json(response))
            } else {
                let error_response = ErrorResponse::from_error(
                    &anyhow::anyhow!("Extension not found"),
                    format!(
                        "Error fetching runtime extensions for graph '{}'",
                        request_payload.graph_id
                    )
                    .as_str(),
                );
                Ok(HttpResponse::NotFound().json(error_response))
            }
        }
        None => {
            let error_response = ErrorResponse {
                status: Status::Fail,
                message: "App not found".to_string(),
                error: None,
            };
            Ok(HttpResponse::NotFound().json(error_response))
        }
    }
}
