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
    use serde_json::json;
    use ten_manager::{
        constants::TEST_DIR,
        designer::{
            graphs::nodes::property::update::{
                update_graph_node_property_endpoint, UpdateGraphNodePropertyRequestPayload,
                UpdateGraphNodePropertyResponsePayload,
            },
            response::{ApiResponse, ErrorResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        graph::graphs_cache_find_by_name,
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use ten_rust::pkg_info::constants::{MANIFEST_JSON_FILENAME, PROPERTY_JSON_FILENAME};
    use uuid::Uuid;

    use crate::test_case::common::mock::{
        inject_all_pkgs_for_mock, inject_all_standard_pkgs_for_mock,
    };

    #[actix_web::test]
    async fn test_update_graph_node_property_success() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let app_manifest_json_str = include_str!("../../../../../test_data/app_manifest.json");
        let app_property_json_str = include_str!("../../../../../test_data/app_property.json");

        // Write input files to temp directory.
        let app_property_json_file_path =
            std::path::Path::new(&temp_dir_path).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&app_property_json_file_path, app_property_json_str).unwrap();

        let app_manifest_json_file_path =
            std::path::Path::new(&temp_dir_path).join(MANIFEST_JSON_FILENAME);
        std::fs::write(&app_manifest_json_file_path, app_manifest_json_str).unwrap();

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
        let all_pkgs_json_str = vec![
            (
                temp_dir_path.clone(),
                app_manifest_json_str.to_string(),
                app_property_json_str.to_string(),
            ),
            (
                format!("{}{}", temp_dir_path.clone(), "/ten_packages/extension/extension_addon_1"),
                include_str!("../../../../../test_data/extension_addon_1_manifest.json")
                    .to_string(),
                "{}".to_string(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json_str)
                    .await;
            assert!(inject_ret.is_ok());
        }

        let graph_id = {
            let graphs_cache = designer_state.graphs_cache.read().await;

            let (id, _) = graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            *id
        };

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/nodes/property/update",
                web::post().to(update_graph_node_property_endpoint),
            ))
            .await;

        // Update property of an existing node.
        let request_payload = UpdateGraphNodePropertyRequestPayload {
            graph_id,
            name: "extension_1".to_string(),
            addon: "extension_addon_1".to_string(),
            extension_group: Some("extension_group_1".to_string()),
            app: Some("http://example.com:8000".to_string()),
            property: Some(json!({
                "key": "updated_value"
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/property/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        println!("Response: {resp:?}");

        // Should succeed with a 200 OK.
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<UpdateGraphNodePropertyResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        let updated_property_json_str =
            std::fs::read_to_string(&app_property_json_file_path).unwrap();

        let expected_property_json_str = include_str!(
            "../../../../../test_data/expected_app_property_after_update_property.json"
        );

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_json_str).unwrap();
        let expected_property: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();

        println!("Updated property: {}", serde_json::to_string_pretty(&updated_property).unwrap());

        // Compare the updated property with the expected property.
        assert_eq!(
            updated_property, expected_property,
            "Updated property does not match expected property"
        );
    }

    #[actix_web::test]
    async fn test_update_graph_node_property_invalid_graph() {
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
            "/api/designer/v1/graphs/nodes/property/update",
            web::post().to(update_graph_node_property_endpoint),
        ))
        .await;

        // Try to update a node in a non-existent graph.
        let request_payload = UpdateGraphNodePropertyRequestPayload {
            graph_id: Uuid::new_v4(),
            name: "node1".to_string(),
            addon: "addon1".to_string(),
            extension_group: None,
            app: Some("http://example.com:8000".to_string()),
            property: Some(json!({
                "key": "updated_value"
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/property/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should fail with a 404 Not Found
        assert_eq!(resp.status(), 404);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Fail);
        assert!(response.message.contains("not found"));
    }

    #[actix_web::test]
    async fn test_update_graph_node_property_node_not_found() {
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

        let graph_id = {
            let graphs_cache = designer_state.graphs_cache.read().await;

            let (id, _) = graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            *id
        };

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/graphs/nodes/property/update",
            web::post().to(update_graph_node_property_endpoint),
        ))
        .await;

        // Try to update a non-existent node
        let request_payload = UpdateGraphNodePropertyRequestPayload {
            graph_id,
            name: "non_existent_node".to_string(),
            addon: "addon1".to_string(),
            extension_group: None,
            app: Some("http://example.com:8000".to_string()),
            property: Some(json!({
                "key": "updated_value"
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/property/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should fail with a 400 Bad Request.
        assert_eq!(resp.status(), 400);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Fail);
        assert!(response.message.contains("not found"));
    }

    #[actix_web::test]
    async fn test_update_graph_node_property_validation_pass() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let app_manifest_json_str = include_str!("../../../../../test_data/app_manifest.json");
        let app_property_json_str = include_str!("../../../../../test_data/app_property.json");

        // Write input files to temp directory.
        let app_property_json_file_path =
            std::path::Path::new(&temp_dir_path).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&app_property_json_file_path, app_property_json_str).unwrap();

        let app_manifest_json_file_path =
            std::path::Path::new(&temp_dir_path).join(MANIFEST_JSON_FILENAME);
        std::fs::write(&app_manifest_json_file_path, app_manifest_json_str).unwrap();

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
        let all_pkgs_json_str = vec![
            (
                temp_dir_path.clone(),
                app_manifest_json_str.to_string(),
                app_property_json_str.to_string(),
            ),
            (
                format!("{}{}", temp_dir_path.clone(), "/ten_packages/extension/extension_addon_1"),
                include_str!("../../../../../test_data/extension_addon_1_manifest.json")
                    .to_string(),
                "{}".to_string(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json_str)
                    .await;
            assert!(inject_ret.is_ok());
        }

        let graph_id = {
            let graphs_cache = designer_state.graphs_cache.read().await;

            let (id, _) = graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            *id
        };

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/nodes/property/update",
                web::post().to(update_graph_node_property_endpoint),
            ))
            .await;

        // Update property of an existing node.
        let request_payload = UpdateGraphNodePropertyRequestPayload {
            graph_id,
            name: "extension_1".to_string(),
            addon: "extension_addon_1".to_string(),
            extension_group: Some("extension_group_1".to_string()),
            app: Some("http://example.com:8000".to_string()),
            property: Some(json!({
                "key": "updated_value"
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/property/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK.
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<UpdateGraphNodePropertyResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        let updated_property_json_str =
            std::fs::read_to_string(&app_property_json_file_path).unwrap();

        let expected_property_json_str = include_str!(
            "../../../../../test_data/expected_app_property_after_update_property.json"
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
    }

    #[actix_web::test]
    async fn test_update_graph_node_property_validation_failure() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let app_manifest_json_str = include_str!("../../../../../test_data/app_manifest.json");
        let app_property_json_str = include_str!("../../../../../test_data/app_property.json");

        // Write input files to temp directory.
        let app_property_json_file_path =
            std::path::Path::new(&temp_dir_path).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&app_property_json_file_path, app_property_json_str).unwrap();

        let app_manifest_json_file_path =
            std::path::Path::new(&temp_dir_path).join(MANIFEST_JSON_FILENAME);
        std::fs::write(&app_manifest_json_file_path, app_manifest_json_str).unwrap();

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
        let all_pkgs_json_str = vec![
            (
                temp_dir_path.clone(),
                app_manifest_json_str.to_string(),
                app_property_json_str.to_string(),
            ),
            (
                format!("{}{}", temp_dir_path.clone(), "/ten_packages/extension/extension_addon_1"),
                include_str!("../../../../../test_data/extension_addon_1_manifest.json")
                    .to_string(),
                "{}".to_string(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json_str)
                    .await;
            assert!(inject_ret.is_ok());
        }

        let graph_id = {
            let graphs_cache = designer_state.graphs_cache.read().await;

            let (id, _) = graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            *id
        };

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/nodes/property/update",
                web::post().to(update_graph_node_property_endpoint),
            ))
            .await;

        // Update property of an existing node.
        let request_payload = UpdateGraphNodePropertyRequestPayload {
            graph_id,
            name: "extension_1".to_string(),
            addon: "extension_addon_1".to_string(),
            extension_group: Some("extension_group_1".to_string()),
            app: Some("http://example.com:8000".to_string()),
            property: Some(json!({
                "key": 32
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/property/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should fail with a 400 Bad Request.
        assert_eq!(resp.status(), 400);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Fail);
        assert!(response.message.contains("validation failed"));

        let updated_property_json_str =
            std::fs::read_to_string(&app_property_json_file_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_json_str).unwrap();
        let expected_property: serde_json::Value =
            serde_json::from_str(app_property_json_str).unwrap();

        // Compare the updated property with the expected property.
        assert_eq!(
            updated_property, expected_property,
            "Updated property does not match expected property"
        );
    }

    #[actix_web::test]
    async fn test_update_graph_node_property_validation_complex_pass() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str = include_str!("../../../../../test_data/app_property.json");
        let input_manifest_json_str = include_str!("../../../../../test_data/app_manifest.json");

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
        let all_pkgs_json_str = vec![
            (
                temp_dir_path.clone(),
                input_manifest_json_str.to_string(),
                input_property_json_str.to_string(),
            ),
            (
                format!("{}{}", temp_dir_path.clone(), "/ten_packages/extension/extension_1"),
                include_str!(
                    "../../../../../test_data/extension_addon_1_with_complex_property_manifest.\
                     json"
                )
                .to_string(),
                "{}".to_string(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json_str)
                    .await;
            assert!(inject_ret.is_ok());
        }

        let graph_id = {
            let graphs_cache = designer_state.graphs_cache.read().await;

            let (id, _) = graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            *id
        };

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/nodes/property/update",
                web::post().to(update_graph_node_property_endpoint),
            ))
            .await;

        // Update property of an existing node.
        let request_payload = UpdateGraphNodePropertyRequestPayload {
            graph_id,
            name: "extension_1".to_string(),
            addon: "extension_addon_1".to_string(),
            extension_group: Some("extension_group_1".to_string()),
            app: Some("http://example.com:8000".to_string()),
            property: Some(json!({
                "key": {
                    "a": 1,
                    "b": "2"
                }
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/property/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK.
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<UpdateGraphNodePropertyResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        let updated_property_json_str = std::fs::read_to_string(&property_path).unwrap();

        let expected_property_json_str = include_str!(
            "../../../../../test_data/expected_app_property_after_update_complex_property.json"
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
    }

    #[actix_web::test]
    async fn test_update_graph_node_property_validation_complex_failure() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str = include_str!("../../../../../test_data/app_property.json");
        let input_manifest_json_str = include_str!("../../../../../test_data/app_manifest.json");

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
        let all_pkgs_json_str = vec![
            (
                temp_dir_path.clone(),
                input_manifest_json_str.to_string(),
                input_property_json_str.to_string(),
            ),
            (
                format!("{}{}", temp_dir_path.clone(), "/ten_packages/extension/extension_1"),
                include_str!(
                    "../../../../../test_data/extension_addon_1_with_complex_property_manifest.\
                     json"
                )
                .to_string(),
                "{}".to_string(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json_str)
                    .await;
            assert!(inject_ret.is_ok());
        }

        let graph_id = {
            let graphs_cache = designer_state.graphs_cache.read().await;

            let (id, _) = graphs_cache_find_by_name(&graphs_cache, "default_with_app_uri").unwrap();

            *id
        };

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/nodes/property/update",
                web::post().to(update_graph_node_property_endpoint),
            ))
            .await;

        // Update property of an existing node with invalid data.
        let request_payload = UpdateGraphNodePropertyRequestPayload {
            graph_id,
            name: "extension_1".to_string(),
            addon: "extension_addon_1".to_string(),
            extension_group: Some("extension_group_1".to_string()),
            app: Some("http://example.com:8000".to_string()),
            property: Some(json!({
                "key": {
                    "a": "wrong",
                    "b": "2"
                }
            })),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/nodes/property/update")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should fail with a 400 Bad Request.
        assert_eq!(resp.status(), 400);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Fail);
        assert!(response.message.contains("validation failed"));

        let updated_property_json_str = std::fs::read_to_string(&property_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_json_str).unwrap();
        let expected_property: serde_json::Value =
            serde_json::from_str(input_property_json_str).unwrap();

        // Compare the updated property with the expected property.
        assert_eq!(
            updated_property, expected_property,
            "Updated property does not match expected property"
        );
    }
}
