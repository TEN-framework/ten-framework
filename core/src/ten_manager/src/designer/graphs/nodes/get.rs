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
use ten_rust::pkg_info::get_pkg_info_for_extension_addon;
use uuid::Uuid;

use ten_rust::graph::node::{GraphNode, GraphNodeType};

use crate::designer::common::{
    get_designer_api_msg_from_pkg, get_designer_property_hashmap_from_pkg,
};
use crate::designer::response::{ApiResponse, ErrorResponse, Status};
use crate::designer::DesignerState;

use super::{get_extension_nodes_in_graph, DesignerApi};

#[derive(Serialize, Deserialize)]
pub struct GetGraphNodesRequestPayload {
    pub graph_id: Uuid,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct GraphNodesSingleResponseData {
    pub addon: String,
    pub name: String,

    // The app which this extension belongs.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app: Option<String>,

    // The extension group which this extension belongs.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub api: Option<DesignerApi>,

    pub property: Option<serde_json::Value>,

    /// This indicates that the extension has been installed under the
    /// `ten_packages/` directory.
    pub is_installed: bool,
}

impl TryFrom<GraphNode> for GraphNodesSingleResponseData {
    type Error = anyhow::Error;

    fn try_from(node: GraphNode) -> Result<Self, Self::Error> {
        if node.type_ != GraphNodeType::Extension {
            return Err(anyhow!(
                "Graph node '{}' is not of type 'extension'",
                node.name
            ));
        }

        Ok(GraphNodesSingleResponseData {
            addon: node.addon.expect("Extension node must have an addon"),
            name: node.name,
            extension_group: node.extension_group,
            app: node.app,
            api: None,
            property: node.property,
            is_installed: false,
        })
    }
}

impl From<GraphNodesSingleResponseData> for GraphNode {
    fn from(designer_extension: GraphNodesSingleResponseData) -> Self {
        GraphNode {
            type_: GraphNodeType::Extension,
            name: designer_extension.name,
            addon: Some(designer_extension.addon),
            extension_group: designer_extension.extension_group,
            app: designer_extension.app,
            property: designer_extension.property,
            source_uri: None,
        }
    }
}

/// Retrieve graph nodes for a specific graph.
pub async fn get_graph_nodes_endpoint(
    request_payload: web::Json<GetGraphNodesRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let pkgs_cache = state.pkgs_cache.read().await;
    let graphs_cache = state.graphs_cache.read().await;

    let graph_id = &request_payload.graph_id;

    let graph_info = graphs_cache.get(&request_payload.graph_id);
    let app_base_dir_of_graph = match graph_info {
        Some(graph_info) => &graph_info.app_base_dir,
        None => &None,
    };

    let extension_graph_nodes =
        match get_extension_nodes_in_graph(graph_id, &graphs_cache) {
            Ok(exts) => exts,
            Err(err) => {
                let error_response = ErrorResponse::from_error(
                    &err,
                    format!(
                        "Error fetching runtime extensions for graph \
                         '{graph_id}'"
                    )
                    .as_str(),
                );
                return Ok(HttpResponse::NotFound().json(error_response));
            }
        };

    let mut resp_extensions: Vec<GraphNodesSingleResponseData> = Vec::new();

    for extension_graph_node in extension_graph_nodes {
        let pkg_info = get_pkg_info_for_extension_addon(
            &pkgs_cache,
            app_base_dir_of_graph,
            &extension_graph_node.app,
            extension_graph_node
                .addon
                .as_ref()
                .expect("Extension node must have an addon"),
        );
        if let Some(pkg_info) = pkg_info {
            resp_extensions.push(GraphNodesSingleResponseData {
                addon: extension_graph_node
                    .addon
                    .clone()
                    .expect("Extension node must have an addon"),
                name: extension_graph_node.name.clone(),
                extension_group: extension_graph_node.extension_group.clone(),
                app: extension_graph_node.app.clone(),
                api: pkg_info.manifest.api.as_ref().map(|api| DesignerApi {
                    property: api
                        .property
                        .as_ref()
                        .filter(|p| !p.is_empty())
                        .map(|p| {
                            get_designer_property_hashmap_from_pkg(p.clone())
                        }),

                    cmd_in: api
                        .cmd_in
                        .as_ref()
                        .filter(|c| !c.is_empty())
                        .map(|c| get_designer_api_msg_from_pkg(c.clone())),

                    cmd_out: api
                        .cmd_out
                        .as_ref()
                        .filter(|c| !c.is_empty())
                        .map(|c| get_designer_api_msg_from_pkg(c.clone())),

                    data_in: api
                        .data_in
                        .as_ref()
                        .filter(|d| !d.is_empty())
                        .map(|d| get_designer_api_msg_from_pkg(d.clone())),

                    data_out: api
                        .data_out
                        .as_ref()
                        .filter(|d| !d.is_empty())
                        .map(|d| get_designer_api_msg_from_pkg(d.clone())),

                    audio_frame_in: api
                        .audio_frame_in
                        .as_ref()
                        .filter(|d| !d.is_empty())
                        .map(|d| get_designer_api_msg_from_pkg(d.clone())),

                    audio_frame_out: api
                        .audio_frame_out
                        .as_ref()
                        .filter(|d| !d.is_empty())
                        .map(|d| get_designer_api_msg_from_pkg(d.clone())),

                    video_frame_in: api
                        .video_frame_in
                        .as_ref()
                        .filter(|d| !d.is_empty())
                        .map(|d| get_designer_api_msg_from_pkg(d.clone())),

                    video_frame_out: api
                        .video_frame_out
                        .as_ref()
                        .filter(|d| !d.is_empty())
                        .map(|d| get_designer_api_msg_from_pkg(d.clone())),
                }),
                property: extension_graph_node.property.clone(),
                is_installed: true,
            });
        } else {
            match GraphNodesSingleResponseData::try_from(
                extension_graph_node.clone(),
            ) {
                Ok(designer_ext) => {
                    resp_extensions.push(designer_ext);
                }
                Err(e) => {
                    let error_response = ErrorResponse::from_error(
                        &e,
                        "This graph node's content is not a valid graph node.",
                    );
                    return Ok(HttpResponse::NotFound().json(error_response));
                }
            }
        }
    }

    let response =
        ApiResponse { status: Status::Ok, data: resp_extensions, meta: None };

    Ok(HttpResponse::Ok().json(response))
}
