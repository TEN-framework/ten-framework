//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use crate::designer::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Serialize, Deserialize)]
pub struct GetGraphsRequestPayload {}

#[derive(Serialize, Deserialize, Debug, PartialEq)]
pub struct GetGraphsResponseData {
    pub uuid: String,
    pub name: Option<String>,
    pub auto_start: Option<bool>,
    pub base_dir: Option<String>,
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
        })
        .collect();

    let response = ApiResponse { status: Status::Ok, data: graphs, meta: None };

    Ok(HttpResponse::Ok().json(response))
}
