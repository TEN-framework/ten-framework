//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use ten_rust::graph::connection::{GraphConnection, GraphMessageFlow};

use crate::designer::response::{ApiResponse, ErrorResponse, Status};
use crate::designer::DesignerState;

use super::DesignerMessageFlow;

#[derive(Serialize, Deserialize)]
pub struct GetGraphConnectionsRequestPayload {
    pub graph_id: Uuid,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct GraphConnectionsSingleResponseData {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app: Option<String>,

    pub extension: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub cmd: Option<Vec<DesignerMessageFlow>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub data: Option<Vec<DesignerMessageFlow>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_frame: Option<Vec<DesignerMessageFlow>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_frame: Option<Vec<DesignerMessageFlow>>,
}

fn get_designer_msg_flow_from_property(
    msg_flow: Vec<GraphMessageFlow>,
) -> Vec<DesignerMessageFlow> {
    if msg_flow.is_empty() {
        return vec![];
    }

    msg_flow.into_iter().map(|v| v.into()).collect()
}

fn get_property_msg_flow_from_designer(
    msg_flow: Vec<DesignerMessageFlow>,
) -> Vec<GraphMessageFlow> {
    msg_flow.into_iter().map(|v| v.into()).collect()
}

impl From<GraphConnection> for GraphConnectionsSingleResponseData {
    fn from(conn: GraphConnection) -> Self {
        GraphConnectionsSingleResponseData {
            app: conn.app,
            extension: conn.extension,

            cmd: conn.cmd.map(get_designer_msg_flow_from_property),

            data: conn.data.map(get_designer_msg_flow_from_property),

            audio_frame: conn
                .audio_frame
                .map(get_designer_msg_flow_from_property),

            video_frame: conn
                .video_frame
                .map(get_designer_msg_flow_from_property),
        }
    }
}

impl From<GraphConnectionsSingleResponseData> for GraphConnection {
    fn from(designer_connection: GraphConnectionsSingleResponseData) -> Self {
        GraphConnection {
            app: designer_connection.app,
            extension: designer_connection.extension,

            cmd: designer_connection
                .cmd
                .map(get_property_msg_flow_from_designer),
            data: designer_connection
                .data
                .map(get_property_msg_flow_from_designer),
            audio_frame: designer_connection
                .audio_frame
                .map(get_property_msg_flow_from_designer),
            video_frame: designer_connection
                .video_frame
                .map(get_property_msg_flow_from_designer),
        }
    }
}

pub async fn get_graph_connections_endpoint(
    request_payload: web::Json<GetGraphConnectionsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let graphs_cache = state.graphs_cache.read().await;

    // Look up the graph directly by UUID from graphs_cache
    if let Some(graph_info) = graphs_cache.get(&request_payload.graph_id) {
        // Convert the connections field to RespConnection.
        let connections: Option<_> = graph_info.graph.connections.as_ref();
        let resp_connections: Vec<GraphConnectionsSingleResponseData> =
            match connections {
                Some(connections) => {
                    connections.iter().map(|conn| conn.clone().into()).collect()
                }
                None => vec![],
            };

        let response = ApiResponse {
            status: Status::Ok,
            data: resp_connections,
            meta: None,
        };

        return Ok(HttpResponse::Ok().json(response));
    }

    let error_response = ErrorResponse {
        status: Status::Fail,
        message: "Graph not found".to_string(),
        error: None,
    };
    Ok(HttpResponse::NotFound().json(error_response))
}
