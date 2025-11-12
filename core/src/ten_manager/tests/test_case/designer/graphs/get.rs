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
            graphs::{
                get::{get_graphs_endpoint, GetGraphsRequestPayload},
                nodes::DesignerGraphNode,
                DesignerGraph, DesignerGraphInfo,
            },
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use uuid::Uuid;

    use crate::test_case::common::mock::{
        inject_all_pkgs_for_mock, inject_all_standard_pkgs_for_mock,
    };

    #[actix_web::test]
    async fn test_get_graphs_success() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state))
                .route("/api/designer/v1/graphs", web::post().to(get_graphs_endpoint)),
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

        let graphs: ApiResponse<Vec<DesignerGraphInfo>> = serde_json::from_str(body_str).unwrap();

        let empty_graph = DesignerGraph {
            nodes: vec![],
            connections: vec![],
            exposed_messages: vec![],
            exposed_properties: vec![],
        };

        #[allow(clippy::useless_vec)]
        let expected_graphs = vec![
            DesignerGraphInfo {
                graph_id: Uuid::parse_str("default").unwrap_or_else(|_| Uuid::new_v4()),
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
                graph_id: Uuid::parse_str("addon_not_found").unwrap_or_else(|_| Uuid::new_v4()),
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
            let expected = expected_map.get(&actual.name).expect("Missing expected graph");
            assert_eq!(actual.name, expected.name);
            assert_eq!(actual.auto_start, expected.auto_start);
            assert_eq!(actual.base_dir, expected.base_dir);
        }

        let json: ApiResponse<Vec<DesignerGraphInfo>> = serde_json::from_str(body_str).unwrap();
        let pretty_json = serde_json::to_string_pretty(&json).unwrap();
        println!("Response body: {pretty_json}");
    }

    #[actix_web::test]
    async fn test_get_graphs_no_app_package() {
        let designer_state = Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        });

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state))
                .route("/api/designer/v1/graphs", web::post().to(get_graphs_endpoint)),
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
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Load the test data from graph_with_selector folder
        let app_manifest_json_str =
            include_str!("../../../test_data/graph_with_selector/manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../test_data/graph_with_selector/property.json").to_string();

        // Create test directory name for the app
        let test_app_dir = "/tmp/test_graph_with_selector".to_string();

        let all_pkgs_json =
            vec![(test_app_dir.clone(), app_manifest_json_str, app_property_json_str)];

        // Inject the test data into caches
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            assert!(inject_ret.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the get_graphs_endpoint
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/graphs", web::post().to(get_graphs_endpoint)),
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
        let response: ApiResponse<Vec<ten_manager::designer::graphs::DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();

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
        let node_names: Vec<&str> = nodes.iter().map(|node| node.get_name()).collect();

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

    #[actix_web::test]
    async fn test_get_graphs_with_sources() {
        // Create a designer state with empty caches
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Load the test data from graph_with_sources folder
        let app_manifest_json_str =
            include_str!("../../../test_data/graph_with_sources/manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../test_data/graph_with_sources/property.json").to_string();

        // Create test directory name for the app
        let test_app_dir = "/tmp/test_graph_with_sources".to_string();

        let all_pkgs_json =
            vec![(test_app_dir.clone(), app_manifest_json_str, app_property_json_str)];

        // Inject the test data into caches
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            assert!(inject_ret.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the get_graphs_endpoint
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/graphs", web::post().to(get_graphs_endpoint)),
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
        let response: ApiResponse<Vec<ten_manager::designer::graphs::DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();

        // Verify the response status
        assert_eq!(response.status, Status::Ok);

        // Verify we got exactly one graph (the "default" graph)
        assert_eq!(response.data.len(), 1);

        let graph_info = &response.data[0];

        // Verify the graph name
        assert_eq!(graph_info.name, Some("default".to_string()));

        // Verify the graph has the expected nodes (2 extensions)
        let nodes = &graph_info.graph.nodes;
        assert_eq!(nodes.len(), 2); // 2 extension nodes

        // Verify specific nodes exist by name
        let node_names: Vec<&str> = nodes.iter().map(|node| node.get_name()).collect();

        // Check for extension nodes
        assert!(node_names.contains(&"test_extension_1"));
        assert!(node_names.contains(&"test_extension_2"));

        // Verify the graph has connections with sources
        let connections = &graph_info.graph.connections;
        assert_eq!(connections.len(), 1);

        // Verify the connection has source information
        let connection = &connections[0];
        assert_eq!(connection.loc.extension, Some("test_extension_2".to_string()));

        // Verify that the connection contains source data
        // The connection should have a cmd with source information
        let cmd_list = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_list.len(), 1);

        let cmd = &cmd_list[0];
        assert_eq!(cmd.name, Some("hello_world".to_string()));

        // Verify the source is properly set
        let sources = &cmd.source;
        assert_eq!(sources.len(), 1);

        let source = &sources[0];
        assert_eq!(source.loc.extension, Some("test_extension_1".to_string()));
        assert_eq!(source.loc.app, Some("msgpack://127.0.0.1:8001/".to_string()));

        // Verify the graph base directory
        assert_eq!(graph_info.base_dir, Some(test_app_dir));

        // Verify auto_start is false (as defined in the test data)
        assert_eq!(graph_info.auto_start, Some(false));
    }

    #[actix_web::test]
    async fn test_get_graphs_with_multiple_sources() {
        // Create a designer state with empty caches
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Load the test data from graph_with_multiple_sources folder
        let app_manifest_json_str =
            include_str!("../../../test_data/graph_with_multiple_sources/manifest.json")
                .to_string();
        let app_property_json_str =
            include_str!("../../../test_data/graph_with_multiple_sources/property.json")
                .to_string();

        // Create test directory name for the app
        let test_app_dir = "/tmp/test_graph_with_multiple_sources".to_string();

        let all_pkgs_json =
            vec![(test_app_dir.clone(), app_manifest_json_str, app_property_json_str)];

        // Inject the test data into caches
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            assert!(inject_ret.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the get_graphs_endpoint
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/graphs", web::post().to(get_graphs_endpoint)),
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
        let response: ApiResponse<Vec<ten_manager::designer::graphs::DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();

        // Verify the response status
        assert_eq!(response.status, Status::Ok);

        // Verify we got exactly one graph (the "default" graph)
        assert_eq!(response.data.len(), 1);

        let graph_info = &response.data[0];

        // Verify the graph name
        assert_eq!(graph_info.name, Some("default".to_string()));

        // Verify the graph has the expected nodes (3 extensions)
        let nodes = &graph_info.graph.nodes;
        // 3 extension nodes: destination_ext, source_ext_1, source_ext_2
        assert_eq!(nodes.len(), 3);

        // Verify specific nodes exist by name
        let node_names: Vec<&str> = nodes.iter().map(|node| node.get_name()).collect();

        // Check for extension nodes
        assert!(node_names.contains(&"destination_ext"));
        assert!(node_names.contains(&"source_ext_1"));
        assert!(node_names.contains(&"source_ext_2"));

        // Verify the graph has connections with multiple sources
        let connections = &graph_info.graph.connections;
        assert_eq!(connections.len(), 1);

        // Verify the connection has multiple source information
        let connection = &connections[0];
        assert_eq!(connection.loc.extension, Some("destination_ext".to_string()));

        // Verify that the connection contains multiple sources data
        // The connection should have a cmd with multiple source information
        let cmd_list = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_list.len(), 1);

        let cmd = &cmd_list[0];
        assert_eq!(cmd.name, Some("multi_source_cmd".to_string()));

        // Verify multiple sources are properly set
        let sources = &cmd.source;
        assert_eq!(sources.len(), 2); // Should have 2 sources

        // Verify first source
        let source_1 = &sources[0];
        assert_eq!(source_1.loc.extension, Some("source_ext_1".to_string()));
        assert_eq!(source_1.loc.app, None); // No app specified in the test data

        // Verify second source
        let source_2 = &sources[1];
        assert_eq!(source_2.loc.extension, Some("source_ext_2".to_string()));
        assert_eq!(source_2.loc.app, None); // No app specified in the test data

        // Verify the graph base directory
        assert_eq!(graph_info.base_dir, Some(test_app_dir));

        // Verify auto_start is false (as defined in the test data)
        assert_eq!(graph_info.auto_start, Some(false));
    }

    #[actix_web::test]
    async fn test_get_graphs_with_subgraph() {
        // Create a designer state with empty caches
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Load the test data from cmd_check_predefined_graph_with_subgraph
        // folder
        let app_manifest_json_str = include_str!(
            "../../../test_data/cmd_check_predefined_graph_with_subgraph/manifest.json"
        )
        .to_string();
        let app_property_json_str = include_str!(
            "../../../test_data/cmd_check_predefined_graph_with_subgraph/property.json"
        )
        .to_string();

        // Create test directory name for the app - use a relative path that
        // works across all platforms (Linux, macOS, Windows)
        let test_app_dir = std::env::current_dir()
            .unwrap()
            .join("tests/test_data/cmd_check_predefined_graph_with_subgraph")
            .to_string_lossy()
            .to_string();

        let all_pkgs_json =
            vec![(test_app_dir.clone(), app_manifest_json_str, app_property_json_str)];

        // Inject the test data into caches
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            if inject_ret.is_err() {
                println!("inject_ret: {inject_ret:?}");
            }
            assert!(inject_ret.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the get_graphs_endpoint
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/graphs", web::post().to(get_graphs_endpoint)),
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
        let response: ApiResponse<Vec<ten_manager::designer::graphs::DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();

        // Verify the response status
        assert_eq!(response.status, Status::Ok);

        // Verify we got exactly one graph (the "default" graph)
        assert_eq!(response.data.len(), 1);

        let graph_info = &response.data[0];

        // Verify the graph name
        assert_eq!(graph_info.name, Some("default".to_string()));

        // After our fix, we should now be using pre_flatten data which
        // preserves subgraph structure
        let nodes = &graph_info.graph.nodes;
        // Verify the graph has the expected nodes (1 extension + 1 subgraph)
        assert_eq!(nodes.len(), 2); // 1 extension node + 1 subgraph node

        // Verify specific nodes exist by name
        let node_names: Vec<&str> = nodes.iter().map(|node| node.get_name()).collect();

        // Check for extension nodes
        assert!(node_names.contains(&"addon_a"));

        // Check for subgraph nodes
        assert!(node_names.contains(&"subgraph_1"));

        // Find the subgraph node and verify it has the resolved graph content
        let subgraph_node = nodes
            .iter()
            .find(|node| {
                if let DesignerGraphNode::Subgraph {
                    content,
                } = node
                {
                    content.name == "subgraph_1"
                } else {
                    false
                }
            })
            .expect("Should have subgraph_1 node");

        if let DesignerGraphNode::Subgraph {
            content,
        } = subgraph_node
        {
            // Verify import_uri is preserved
            assert_eq!(content.graph.import_uri, "graphs/test_graph.json");

            // Verify that the graph field is populated with the resolved
            // content
            let resolved_graph =
                content.graph.graph.as_ref().expect("Subgraph should have resolved graph content");

            // Verify the resolved graph has the expected nodes from
            // test_graph.json
            assert_eq!(resolved_graph.nodes.len(), 2); // addon_b and addon_c

            let resolved_node_names: Vec<&str> =
                resolved_graph.nodes.iter().map(|node| node.get_name()).collect();

            assert!(resolved_node_names.contains(&"addon_b"));
            assert!(resolved_node_names.contains(&"addon_c"));

            // Verify connections in the resolved graph
            assert_eq!(resolved_graph.connections.len(), 1);

            // Verify exposed messages
            assert_eq!(resolved_graph.exposed_messages.len(), 1);
            let exposed_msg = &resolved_graph.exposed_messages[0];
            assert_eq!(exposed_msg.name, "C");
            assert_eq!(exposed_msg.extension, Some("addon_b".to_string()));

            // Verify exposed properties
            assert_eq!(resolved_graph.exposed_properties.len(), 1);
            let exposed_prop = &resolved_graph.exposed_properties[0];
            assert_eq!(exposed_prop.name, "key");
            assert_eq!(exposed_prop.extension, Some("addon_c".to_string()));
        }

        // Verify the graph has connections
        let connections = &graph_info.graph.connections;
        assert_eq!(connections.len(), 1);

        // Verify the connection goes from addon_a to subgraph_1
        let connection = &connections[0];
        assert_eq!(connection.loc.extension, Some("addon_a".to_string()));
        assert_eq!(connection.cmd.as_ref().unwrap().len(), 1);
        assert_eq!(connection.cmd.as_ref().unwrap()[0].name, Some("C".to_string()));
        assert_eq!(connection.cmd.as_ref().unwrap()[0].dest.len(), 1);
        assert_eq!(connection.cmd.as_ref().unwrap()[0].dest[0].loc.extension, None);
        assert_eq!(
            connection.cmd.as_ref().unwrap()[0].dest[0].loc.subgraph,
            Some("subgraph_1".to_string())
        );
        assert_eq!(connection.cmd.as_ref().unwrap()[0].dest[0].loc.app, None);

        // Verify the graph base directory
        assert_eq!(graph_info.base_dir, Some(test_app_dir));

        // Verify auto_start is false (as defined in the test data)
        assert_eq!(graph_info.auto_start, Some(false));
    }
}
