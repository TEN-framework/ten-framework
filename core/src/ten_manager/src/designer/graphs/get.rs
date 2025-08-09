//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use crate::designer::graphs::nodes::get::DesignerGraphNode;
use crate::designer::graphs::DesignerGraphInfo;
use crate::designer::{
    graphs::{
        DesignerGraph, DesignerGraphExposedMessage,
        DesignerGraphExposedProperty,
    },
    response::{ApiResponse, Status},
    DesignerState,
};
use ten_rust::graph::{
    connection::GraphConnection, graph_info::GraphInfo, node::GraphNode,
    GraphExposedMessage, GraphExposedProperty,
};

#[derive(Serialize, Deserialize)]
pub struct GetGraphsRequestPayload {}

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

/// Extracts a DesignerGraph from GraphInfo, preferring pre_flatten data if
/// available, otherwise falling back to the main graph data.
fn extract_designer_graph_from_graph_info(
    graph_info: &GraphInfo,
) -> DesignerGraph {
    graph_info
        .graph
        .graph
        .pre_flatten
        .as_ref()
        .map(|pre_flatten| {
            create_designer_graph(
                &pre_flatten.nodes,
                &pre_flatten.connections,
                &pre_flatten.exposed_messages,
                &pre_flatten.exposed_properties,
            )
        })
        .unwrap_or_else(|| {
            create_designer_graph(
                &graph_info.graph.graph.nodes,
                &graph_info.graph.graph.connections,
                &graph_info.graph.graph.exposed_messages,
                &graph_info.graph.graph.exposed_properties,
            )
        })
}

pub async fn get_graphs_endpoint(
    _request_payload: web::Json<GetGraphsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let graphs_cache = state.graphs_cache.read().await;

    let graphs: Vec<DesignerGraphInfo> = graphs_cache
        .iter()
        .map(|(uuid, graph_info)| DesignerGraphInfo {
            graph_id: *uuid,
            name: graph_info.name.clone(),
            auto_start: graph_info.auto_start,
            base_dir: graph_info.app_base_dir.clone(),
            graph: extract_designer_graph_from_graph_info(graph_info),
        })
        .collect();

    let response = ApiResponse { status: Status::Ok, data: graphs, meta: None };

    Ok(HttpResponse::Ok().json(response))
}
