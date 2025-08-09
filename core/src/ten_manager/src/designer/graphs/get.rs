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
use crate::designer::{
    graphs::{
        DesignerGraph, DesignerGraphExposedMessage,
        DesignerGraphExposedProperty,
    },
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Serialize, Deserialize)]
pub struct GetGraphsRequestPayload {}

#[derive(Serialize, Deserialize, Debug)]
pub struct GetGraphsResponseData {
    pub uuid: String,
    pub name: Option<String>,
    pub auto_start: Option<bool>,
    pub base_dir: Option<String>,
    pub graph: DesignerGraph,
}

pub async fn get_graphs_endpoint(
    _request_payload: web::Json<GetGraphsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let graphs_cache = state.graphs_cache.read().await;

    let graphs: Vec<GetGraphsResponseData> = graphs_cache
        .iter()
        .map(|(uuid, graph_info)| GetGraphsResponseData {
            uuid: uuid.to_string(),
            name: graph_info.name.clone(),
            auto_start: graph_info.auto_start,
            base_dir: graph_info.app_base_dir.clone(),
            graph: graph_info
                .graph
                .graph
                .pre_flatten
                .as_ref()
                .map(|pre_flatten| DesignerGraph {
                    nodes: pre_flatten
                        .nodes
                        .iter()
                        .filter_map(|node| {
                            DesignerGraphNode::try_from(node.clone()).ok()
                        })
                        .collect(),
                    connections: pre_flatten
                        .connections
                        .as_ref()
                        .map(|conns| {
                            conns
                                .iter()
                                .map(|conn| conn.clone().into())
                                .collect()
                        })
                        .unwrap_or_default(),
                    exposed_messages: pre_flatten
                        .exposed_messages
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
                    exposed_properties: pre_flatten
                        .exposed_properties
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
                })
                .unwrap_or_else(|| DesignerGraph {
                    nodes: graph_info
                        .graph
                        .graph
                        .nodes
                        .iter()
                        .filter_map(|node| {
                            DesignerGraphNode::try_from(node.clone()).ok()
                        })
                        .collect(),
                    connections: graph_info
                        .graph
                        .graph
                        .connections
                        .as_ref()
                        .map(|conns| {
                            conns
                                .iter()
                                .map(|conn| conn.clone().into())
                                .collect()
                        })
                        .unwrap_or_default(),
                    exposed_messages: graph_info
                        .graph
                        .graph
                        .exposed_messages
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
                    exposed_properties: graph_info
                        .graph
                        .graph
                        .exposed_properties
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
                }),
        })
        .collect();

    let response = ApiResponse { status: Status::Ok, data: graphs, meta: None };

    Ok(HttpResponse::Ok().json(response))
}
