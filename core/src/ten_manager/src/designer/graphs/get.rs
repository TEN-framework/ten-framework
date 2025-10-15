//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, path::Path, sync::Arc};

use actix_web::{web, HttpResponse, Responder};
use futures::future::try_join_all;
use serde::{Deserialize, Serialize};
use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp,
    graph::{
        connection::GraphConnection, graph_info::GraphInfo, node::GraphNode, Graph,
        GraphExposedMessage, GraphExposedProperty,
    },
    pkg_info::get_pkg_info_for_extension_addon,
};

use crate::designer::{
    common::{get_designer_api_msg_from_pkg, get_designer_api_property_from_pkg},
    graphs::{
        nodes::{DesignerApi, DesignerGraphNode},
        DesignerGraph, DesignerGraphExposedMessage, DesignerGraphExposedProperty,
        DesignerGraphInfo,
    },
    response::{ApiResponse, ErrorResponse, Status},
    DesignerState,
};

#[derive(Serialize, Deserialize)]
pub struct GetGraphsRequestPayload {}

/// Loads a graph from the given import_uri relative to the base_dir.
fn load_graph_from_import_uri(
    import_uri: &str,
    base_dir: &Option<String>,
) -> Option<DesignerGraph> {
    let base_path = base_dir.as_ref()?;
    let graph_path = Path::new(base_path).join(import_uri);

    // Read the graph file
    let graph_content = std::fs::read_to_string(&graph_path).ok()?;

    // Parse the graph JSON
    let graph: Graph = serde_json::from_str(&graph_content).ok()?;

    // Convert to DesignerGraph
    Some(create_designer_graph(
        &graph.nodes,
        &graph.connections,
        &graph.exposed_messages,
        &graph.exposed_properties,
    ))
}

/// Resolves import_uri in subgraph nodes and populates the graph field.
fn resolve_subgraph_imports(
    mut designer_graph: DesignerGraph,
    base_dir: &Option<String>,
) -> DesignerGraph {
    for node in &mut designer_graph.nodes {
        if let DesignerGraphNode::Subgraph {
            content,
        } = node
        {
            if content.graph.graph.is_none() {
                // Load the graph from import_uri
                content.graph.graph =
                    load_graph_from_import_uri(&content.graph.import_uri, base_dir);
            }
        }
    }
    designer_graph
}

/// Converts a collection of nodes, connections, exposed_messages, and
/// exposed_properties into a DesignerGraph structure.
fn create_designer_graph(
    nodes: &[GraphNode],
    connections: &Option<Vec<GraphConnection>>,
    exposed_messages: &Option<Vec<GraphExposedMessage>>,
    exposed_properties: &Option<Vec<GraphExposedProperty>>,
) -> DesignerGraph {
    DesignerGraph {
        nodes: nodes
            .iter()
            .filter_map(|node| DesignerGraphNode::try_from(node.clone()).ok())
            .collect(),
        connections: connections
            .as_ref()
            .map(|conns| conns.iter().map(|conn| conn.clone().into()).collect())
            .unwrap_or_default(),
        exposed_messages: exposed_messages
            .as_ref()
            .map(|msgs| {
                msgs.iter()
                    .map(|msg| DesignerGraphExposedMessage {
                        msg_type: msg.msg_type.clone(),
                        name: msg.name.clone(),
                        extension: msg.extension.clone(),
                        subgraph: msg.subgraph.clone(),
                    })
                    .collect()
            })
            .unwrap_or_default(),
        exposed_properties: exposed_properties
            .as_ref()
            .map(|props| {
                props
                    .iter()
                    .map(|prop| DesignerGraphExposedProperty {
                        extension: prop.extension.clone(),
                        subgraph: prop.subgraph.clone(),
                        name: prop.name.clone(),
                    })
                    .collect()
            })
            .unwrap_or_default(),
    }
}

/// Extracts a DesignerGraph from GraphInfo.
async fn extract_designer_graph_from_graph_info(
    graph_info: &GraphInfo,
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
) -> Result<DesignerGraph, ErrorResponse> {
    let mut designer_graph = create_designer_graph(
        &graph_info.graph.graph.nodes,
        &graph_info.graph.graph.connections,
        &graph_info.graph.graph.exposed_messages,
        &graph_info.graph.graph.exposed_properties,
    );

    // Resolve subgraph imports
    designer_graph = resolve_subgraph_imports(designer_graph, &graph_info.app_base_dir);

    // Update the api and installation status of the nodes
    for node in &mut designer_graph.nodes {
        if let DesignerGraphNode::Extension {
            content,
        } = node
        {
            let pkg_info = get_pkg_info_for_extension_addon(
                pkgs_cache,
                &graph_info.app_base_dir,
                &content.app,
                &content.addon,
            );

            if let Some(pkg_info) = pkg_info {
                let manifest_api =
                    pkg_info.manifest.get_flattened_api(graph_info.app_base_dir.as_deref()).await;
                if manifest_api.is_err() {
                    let error_response = ErrorResponse::from_error(
                        &manifest_api.err().unwrap(),
                        "Failed to flatten API for extension",
                    );
                    return Err(error_response);
                }

                let manifest_api = manifest_api.unwrap();
                if let Some(api) = manifest_api {
                    content.api = Some(DesignerApi {
                        property: api
                            .property
                            .as_ref()
                            .filter(|p| !p.is_empty())
                            .map(|p| get_designer_api_property_from_pkg(p.clone())),

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
                    });
                }

                content.is_installed = true;
            }
        }
    }

    Ok(designer_graph)
}

pub async fn get_graphs_endpoint(
    _request_payload: web::Json<GetGraphsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let pkgs_cache = state.pkgs_cache.read().await;
    let graphs_cache = state.graphs_cache.read().await;

    let pkgs_cache_clone = pkgs_cache.clone();
    let graph_futures: Vec<_> = graphs_cache
        .iter()
        .map(|(uuid, graph_info)| {
            let pkgs_cache_inner = pkgs_cache_clone.clone();
            async move {
                let graph_result =
                    extract_designer_graph_from_graph_info(graph_info, &pkgs_cache_inner).await?;

                Ok::<_, actix_web::Error>(DesignerGraphInfo {
                    graph_id: *uuid,
                    name: graph_info.name.clone(),
                    auto_start: graph_info.auto_start,
                    base_dir: graph_info.app_base_dir.clone(),
                    graph: graph_result,
                })
            }
        })
        .collect();

    let graphs = try_join_all(graph_futures).await?;

    let response = ApiResponse {
        status: Status::Ok,
        data: graphs,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
