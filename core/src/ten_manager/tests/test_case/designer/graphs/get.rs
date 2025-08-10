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
    use uuid::Uuid;

    use ten_manager::{
        constants::TEST_DIR,
        designer::{
            graphs::{
                get::{get_graphs_endpoint, GetGraphsRequestPayload},
                DesignerGraph, DesignerGraphInfo,
            },
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    use crate::test_case::common::mock::{
        inject_all_pkgs_for_mock, inject_all_standard_pkgs_for_mock,
    };

    #[actix_web::test]
    async fn test_get_graphs_success() {
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
            )
            .await;
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs",
                web::post().to(get_graphs_endpoint),
            ),
        )
        .await;

        let request_payload = GetGraphsRequestPayload {};

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let graphs: ApiResponse<Vec<DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();

        let empty_graph = DesignerGraph {
            nodes: vec![],
            connections: vec![],
            exposed_messages: vec![],
            exposed_properties: vec![],
        };

        let expected_graphs = vec![
            DesignerGraphInfo {
                graph_id: Uuid::parse_str("default")
                    .unwrap_or_else(|_| Uuid::new_v4()),
                name: Some("default".to_string()),
                auto_start: Some(true),
                base_dir: Some(TEST_DIR.to_string()),
                graph: empty_graph.clone(),
            },
            DesignerGraphInfo {
                graph_id: Uuid::parse_str("default_with_app_uri")
                    .unwrap_or_else(|_| Uuid::new_v4()),
                name: Some("default_with_app_uri".to_string()),
                auto_start: Some(true),
                base_dir: Some(TEST_DIR.to_string()),
                graph: empty_graph.clone(),
            },
            DesignerGraphInfo {
                graph_id: Uuid::parse_str("addon_not_found")
                    .unwrap_or_else(|_| Uuid::new_v4()),
                name: Some("addon_not_found".to_string()),
                auto_start: Some(false),
                base_dir: Some(TEST_DIR.to_string()),
                graph: empty_graph.clone(),
            },
        ];

        assert_eq!(graphs.data.len(), expected_graphs.len());

        // Create a map of expected graphs by name for easier lookup.
        let expected_map: HashMap<_, _> =
            expected_graphs.iter().map(|g| (g.name.clone(), g)).collect();

        for actual in graphs.data.iter() {
            let expected =
                expected_map.get(&actual.name).expect("Missing expected graph");
            assert_eq!(actual.name, expected.name);
            assert_eq!(actual.auto_start, expected.auto_start);
            assert_eq!(actual.base_dir, expected.base_dir);
        }

        let json: ApiResponse<Vec<DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();
        let pretty_json = serde_json::to_string_pretty(&json).unwrap();
        println!("Response body: {pretty_json}");
    }

    #[actix_web::test]
    async fn test_get_graphs_no_app_package() {
        let designer_state = Arc::new(DesignerState {
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
        });

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs",
                web::post().to(get_graphs_endpoint),
            ),
        )
        .await;

        let request_payload = GetGraphsRequestPayload {};

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());
        println!("Response body: {}", resp.status());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");
    }

    #[actix_web::test]
    async fn test_get_graphs_with_selector() {
        // Create a designer state with empty caches
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

        // Load the test data from graph_with_selector folder
        let app_manifest_json_str = include_str!(
            "../../../test_data/graph_with_selector/manifest.json"
        )
        .to_string();
        let app_property_json_str = include_str!(
            "../../../test_data/graph_with_selector/property.json"
        )
        .to_string();

        // Create test directory name for the app
        let test_app_dir = "/tmp/test_graph_with_selector".to_string();

        let all_pkgs_json = vec![(
            test_app_dir.clone(),
            app_manifest_json_str,
            app_property_json_str,
        )];

        // Inject the test data into caches
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            )
            .await;
            assert!(inject_ret.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the get_graphs_endpoint
        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs",
                web::post().to(get_graphs_endpoint),
            ),
        )
        .await;

        // Create a request payload
        let request_payload = GetGraphsRequestPayload {};

        // Make the request
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Assert that the response is successful
        assert!(resp.status().is_success());

        // Get the response body
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        // Parse the response
        let response: ApiResponse<
            Vec<ten_manager::designer::graphs::DesignerGraphInfo>,
        > = serde_json::from_str(body_str).unwrap();

        // Verify the response status
        assert_eq!(response.status, Status::Ok);

        // Verify we got exactly one graph (the "default" graph)
        assert_eq!(response.data.len(), 1);

        let graph_info = &response.data[0];

        // Verify the graph name
        assert_eq!(graph_info.name, Some("default".to_string()));

        // Verify the graph has the expected nodes (extensions and selectors)
        let nodes = &graph_info.graph.nodes;
        assert_eq!(nodes.len(), 7); // All 7 nodes: 4 extensions + 3 selectors

        // Verify all nodes are properly converted
        assert_eq!(nodes.len(), 7);

        // Verify specific nodes exist by name
        let node_names: Vec<&str> =
            nodes.iter().map(|node| node.get_name()).collect();

        // Check for extension nodes
        assert!(node_names.contains(&"test_extension_1"));
        assert!(node_names.contains(&"test_extension_2"));
        assert!(node_names.contains(&"test_extension_3"));
        assert!(node_names.contains(&"test_extension_4"));

        // Check for selector nodes
        assert!(node_names.contains(&"selector_for_ext_1_and_2"));
        assert!(node_names.contains(&"selector_for_ext_1_and_2_and_3"));
        assert!(node_names.contains(&"selector_for_ext_1_or_3"));

        // Verify the graph has connections
        let connections = &graph_info.graph.connections;
        assert_eq!(connections.len(), 2);

        // Verify the graph base directory
        assert_eq!(graph_info.base_dir, Some(test_app_dir));

        // Verify auto_start is false (as defined in the test data)
        assert_eq!(graph_info.auto_start, Some(false));
    }
}
