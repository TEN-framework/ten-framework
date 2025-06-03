//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::{collections::HashMap, sync::Arc};

    use actix_web::{test, web, App};
    use ten_manager::{
        constants::TEST_DIR,
        designer::{
            graphs::nodes::get::{
                get_graph_nodes_endpoint, GetGraphNodesRequestPayload,
                GraphNodesSingleResponseData,
            },
            response::{ApiResponse, ErrorResponse},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use uuid::Uuid;

    use crate::test_case::common::mock::inject_all_standard_pkgs_for_mock;

    #[actix_web::test]
    async fn test_get_extensions_success() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(
                TmanConfig::default(),
            )),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(
                TmanStorageInMemory::default(),
            )),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            inject_all_standard_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                TEST_DIR,
            );
        }

        let designer_state = Arc::new(designer_state);

        // Find the uuid of the "default" graph.
        let graph_id = {
            let graphs_cache = designer_state.graphs_cache.read().await;

            graphs_cache
                .iter()
                .find_map(|(uuid, info)| {
                    if info
                        .name
                        .as_ref()
                        .map(|name| name == "default")
                        .unwrap_or(false)
                    {
                        Some(*uuid)
                    } else {
                        None
                    }
                })
                .expect("Default graph should exist")
        };

        let request_payload = GetGraphNodesRequestPayload { graph_id };

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs/nodes",
                web::post().to(get_graph_nodes_endpoint),
            ),
        )
        .await;

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        eprintln!("body_str: {body_str}");

        let extensions: ApiResponse<Vec<GraphNodesSingleResponseData>> =
            serde_json::from_str(body_str).unwrap();

        assert!(!extensions.data.is_empty());

        let json: ApiResponse<Vec<GraphNodesSingleResponseData>> =
            serde_json::from_str(body_str).unwrap();
        let pretty_json = serde_json::to_string_pretty(&json).unwrap();
        println!("Response body: {pretty_json}");

        let expected_response_json_str = include_str!(
            "../../../../test_data/get_extension_info/response.json"
        );

        let expected_response_json: serde_json::Value =
            serde_json::from_str(expected_response_json_str).unwrap();

        assert_eq!(
            expected_response_json,
            serde_json::to_value(&json.data).unwrap(),
            "Response does not match expected response"
        );
    }

    #[actix_web::test]
    async fn test_get_extensions_no_graph() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(
                TmanConfig::default(),
            )),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(
                TmanStorageInMemory::default(),
            )),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            inject_all_standard_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                TEST_DIR,
            );
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs/nodes",
                web::post().to(get_graph_nodes_endpoint),
            ),
        )
        .await;

        // Use a random UUID that doesn't exist in the graphs_cache.
        let request_payload =
            GetGraphNodesRequestPayload { graph_id: Uuid::new_v4() };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(!resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        let error_response: ErrorResponse =
            serde_json::from_str(body_str).unwrap();
        assert!(error_response.message.contains("not found in graph caches"));
    }
}
