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
            graphs::nodes::{
                add::{add_graph_node_endpoint, AddGraphNodeRequestPayload},
                delete::{
                    delete_graph_node_endpoint, DeleteGraphNodeRequestPayload,
                    DeleteGraphNodeResponsePayload,
                },
            },
            response::{ApiResponse, ErrorResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        graph::{graphs_cache_find_by_id, graphs_cache_find_by_name},
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use ten_rust::{
        graph::node::GraphNode,
        pkg_info::constants::{MANIFEST_JSON_FILENAME, PROPERTY_JSON_FILENAME},
    };
    use uuid::Uuid;

    use crate::test_case::common::mock::{
        inject_all_pkgs_for_mock, inject_all_standard_pkgs_for_mock,
    };

    #[actix_web::test]
    async fn test_delete_graph_node_invalid_graph() {
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

        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/graphs/nodes/delete",
            web::post().to(delete_graph_node_endpoint),
        ))
        .await;

        // Try to delete a node from a non-existent graph.
        let request_payload = DeleteGraphNodeRequestPayload {
            graph_id: Uuid::new_v4(),
            name: "test_node".to_string(),
            addon: "test_addon".to_string(),
            extension_group: None,
            app: Some("http://test-app-uri.com".to_string()),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should fail with a 404 Not Found.
        assert_eq!(resp.status(), 404);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Fail);
        assert!(response.message.contains("Graph not found"));
    }

    #[actix_web::test]
    async fn test_delete_graph_node_nonexistent_node() {
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

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;

            let (graph_id, _) =
                graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/graphs/nodes/delete",
            web::post().to(delete_graph_node_endpoint),
        ))
        .await;

        // Try to delete a non-existent node from an existing graph.
        let request_payload = DeleteGraphNodeRequestPayload {
            graph_id: graph_id_clone,
            name: "non_existent_node".to_string(),
            addon: "test_addon".to_string(),
            extension_group: None,
            app: Some("http://example.com:8000".to_string()),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with 200 OK, since deleting a non-existent node is not
        // an error (the node is already gone).
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphNodeResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);
    }

    #[actix_web::test]
    async fn test_delete_graph_node_success() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str = include_str!("../../../../test_data/app_property.json");
        let input_manifest_json_str = include_str!("../../../../test_data/app_manifest.json");

        // Write input files to temp directory.
        let property_path = std::path::Path::new(&temp_dir_path).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, input_property_json_str).unwrap();

        let manifest_path = std::path::Path::new(&temp_dir_path).join(MANIFEST_JSON_FILENAME);
        std::fs::write(&manifest_path, input_manifest_json_str).unwrap();

        // Initialize test state.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Inject the test app into the mock.
        let all_pkgs_json = vec![(
            temp_dir_path.clone(),
            std::fs::read_to_string(&manifest_path).unwrap(),
            std::fs::read_to_string(&property_path).unwrap(),
        )];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            assert!(inject_ret.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) =
                graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        // First add a node, then delete it.
        // Setup the add endpoint.
        let app_add =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/nodes/add",
                web::post().to(add_graph_node_endpoint),
            ))
            .await;

        // Add a node to the default graph.
        let add_request_payload = AddGraphNodeRequestPayload {
            graph_id: graph_id_clone,
            name: "test_delete_node".to_string(),
            addon: "test_addon".to_string(),
            extension_group: None,
            app: Some("http://example.com:8000".to_string()),
            property: Some(serde_json::json!({
                "test_property": "test_value_for_delete"
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/add")
            .set_json(add_request_payload)
            .to_request();
        let resp = test::call_service(&app_add, req).await;

        // Ensure add was successful.
        assert_eq!(resp.status(), 200);

        let updated_property_json_str = std::fs::read_to_string(&property_path).unwrap();

        let expected_property_json_str = include_str!(
            "../../../../test_data/\
             expected_property_after_adding_in_test_delete_graph_node_success.json"
        );

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_json_str).unwrap();
        let expected_property: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();

        // Compare the updated property with the expected property.
        assert_eq!(
            updated_property, expected_property,
            "Updated property does not match expected property"
        );

        // Setup the delete endpoint.
        let app_delete =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/nodes/delete",
                web::post().to(delete_graph_node_endpoint),
            ))
            .await;

        // Now delete the node we just added.
        let delete_request_payload = DeleteGraphNodeRequestPayload {
            graph_id: graph_id_clone,
            name: "test_delete_node".to_string(),
            addon: "test_addon".to_string(),
            extension_group: None,
            app: Some("http://example.com:8000".to_string()),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/delete")
            .set_json(delete_request_payload)
            .to_request();
        let resp = test::call_service(&app_delete, req).await;

        // Should succeed with a 200 OK.
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphNodeResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // Verify the node was actually removed from the data.
        let graphs_cache = designer_state.graphs_cache.read().await;

        if let Some(graph_info) = graphs_cache_find_by_id(&graphs_cache, &graph_id_clone) {
            // Check if the node is gone.
            let node_exists = graph_info.graph.nodes().iter().any(|node| match node {
                GraphNode::Extension {
                    content,
                } => content.name == "test_delete_node" && content.addon == "test_addon",
                _ => false,
            });
            assert!(!node_exists, "Node should have been deleted");
        } else {
            panic!("Graph 'default_with_app_uri' not found");
        }

        let updated_property_content = std::fs::read_to_string(&property_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_content).unwrap();
        let expected_property: serde_json::Value =
            serde_json::from_str(input_property_json_str).unwrap();

        // Compare the updated property with the expected property
        assert_eq!(
            updated_property, expected_property,
            "Updated property does not match expected property"
        );
    }
}
