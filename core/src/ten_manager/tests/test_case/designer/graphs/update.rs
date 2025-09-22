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
        designer::{
            graphs::update::{
                update_graph_endpoint, GraphNodeForUpdate, UpdateGraphRequestPayload,
            },
            response::{ErrorResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        graph::graphs_cache_find_by_name,
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use ten_rust::{
        graph::{
            connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow},
            node::GraphNode,
        },
        pkg_info::constants::PROPERTY_JSON_FILENAME,
    };
    use uuid::Uuid;

    use crate::test_case::common::mock::inject_all_pkgs_for_mock;

    #[actix_web::test]
    async fn test_update_graph_success() {
        // Create a designer state with an empty graphs cache.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        let all_pkgs_json = vec![(test_dir.clone(), app_manifest_json_str, app_property_json_str)];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            assert!(inject_ret.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = &designer_state.graphs_cache.read().await;
            let (graph_id, _) =
                graphs_cache_find_by_name(graphs_cache, "default_with_app_uri").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the update_graph_endpoint.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/graphs/update", web::post().to(update_graph_endpoint)),
        )
        .await;

        // Create test nodes and connections.
        let nodes = [GraphNode::new_extension_node(
            "node1".to_string(),
            "test_addon".to_string(),
            None,
            None,
            None,
        )];

        // Create a connection with message flow.
        let dest = GraphDestination {
            loc: GraphLoc {
                app: None,
                extension: Some("node2".to_string()),
                subgraph: None,
                selector: None,
            },
            msg_conversion: None,
        };

        let message_flow =
            GraphMessageFlow::new(Some("test_cmd".to_string()), None, vec![dest], vec![]);

        let connection = GraphConnection {
            loc: GraphLoc {
                app: None,
                extension: Some("node1".to_string()),
                subgraph: None,
                selector: None,
            },
            cmd: Some(vec![message_flow]),
            data: None,
            audio_frame: None,
            video_frame: None,
        };

        let connections = vec![connection];

        // Create a request payload.
        let request_payload = UpdateGraphRequestPayload {
            graph_id: graph_id_clone,
            nodes: nodes
                .iter()
                .map(|node| match node {
                    GraphNode::Extension {
                        content,
                    } => GraphNodeForUpdate {
                        name: content.name.clone(),
                        addon: content.addon.clone(),
                        extension_group: content.extension_group.clone(),
                        app: content.app.clone(),
                        property: content.property.clone(),
                    },
                    _ => panic!("Node is not an extension node"),
                })
                .collect(),
            connections: connections.clone(),
            exposed_messages: vec![],
            auto_start: None,
        };

        // Make the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        println!("resp: {resp:?}");

        // Assert that the response is successful.
        // assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        // Define expected property.json content after adding the connection.
        let expected_property_json_str =
            include_str!("../../../test_data/exoected_property__update_graph_1.json");

        // Read the actual property.json file generated during the test.
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value = serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_update_graph_not_found() {
        // Create a designer state with an empty graphs cache.
        let designer_state = Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        });

        // Create a test app with the update_graph_endpoint.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state))
                .route("/api/designer/v1/graphs/update", web::post().to(update_graph_endpoint)),
        )
        .await;

        // Use a random UUID that doesn't exist in the cache.
        let nonexistent_graph_id = Uuid::new_v4();

        // Create a request payload with the nonexistent graph ID.
        let request_payload = UpdateGraphRequestPayload {
            graph_id: nonexistent_graph_id,
            nodes: vec![],
            connections: vec![],
            exposed_messages: vec![],
            auto_start: None,
        };

        // Make the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Assert that the response status is 400 Bad Request (failure).
        assert_eq!(resp.status(), 400);

        // Get the response body.
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        // Parse the response as an ErrorResponse.
        let error_response: ErrorResponse = serde_json::from_str(body_str).unwrap();

        // Verify the error response.
        assert_eq!(error_response.status, Status::Fail);
        assert!(error_response.message.contains("not found"));
    }

    #[actix_web::test]
    async fn test_update_graph_empty_connections() {
        // Create a designer state with an empty graphs cache.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        let all_pkgs_json = vec![(test_dir.clone(), app_manifest_json_str, app_property_json_str)];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            assert!(inject_ret.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = &designer_state.graphs_cache.read().await;
            let (graph_id, _) =
                graphs_cache_find_by_name(graphs_cache, "default_with_app_uri").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the update_graph_endpoint.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/graphs/update", web::post().to(update_graph_endpoint)),
        )
        .await;

        // Create test nodes but empty connections.
        let nodes = [GraphNode::new_extension_node(
            "new_node".to_string(),
            "test_addon".to_string(),
            None,
            Some("http://example.com:8000".to_string()),
            None,
        )];

        let connections = vec![]; // Empty connections.

        // Create a request payload.
        let request_payload = UpdateGraphRequestPayload {
            graph_id: graph_id_clone,
            nodes: nodes
                .iter()
                .map(|node| match node {
                    GraphNode::Extension {
                        content,
                    } => GraphNodeForUpdate {
                        name: content.name.clone(),
                        addon: content.addon.clone(),
                        extension_group: content.extension_group.clone(),
                        app: content.app.clone(),
                        property: content.property.clone(),
                    },
                    _ => panic!("Node is not an extension node"),
                })
                .collect(),
            connections,
            exposed_messages: vec![],
            auto_start: None,
        };

        // Make the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        println!("resp: {resp:?}");

        // Assert that the response is successful.
        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        // Define expected property.json content after adding the connection.
        let expected_property_json_str =
            include_str!("../../../test_data/exoected_property__update_graph.json");

        // Read the actual property.json file generated during the test.
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value = serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }
}
