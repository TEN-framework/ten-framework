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
            graphs::connections::add::{
                add_graph_connection_endpoint,
                AddGraphConnectionRequestPayload,
                AddGraphConnectionResponsePayload,
            },
            response::ApiResponse,
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        fs::copy_folder_recursively,
        graph::graphs_cache_find_by_name,
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
        pkg_info::get_all_pkgs::get_all_pkgs_in_app,
    };
    use ten_rust::pkg_info::{
        constants::PROPERTY_JSON_FILENAME, message::MsgType,
    };
    use uuid::Uuid;

    use crate::test_case::common::mock::inject_all_pkgs_for_mock;

    #[actix_web::test]
    async fn test_add_graph_connection_success_1() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings.
        let ext1_manifest = include_str!(
            "../../../../test_data/extension_addon_1_manifest.json"
        )
        .to_string();

        let ext2_manifest = include_str!(
            "../../../../test_data/extension_addon_2_manifest.json"
        )
        .to_string();

        let ext3_manifest = include_str!(
            "../../../../test_data/extension_addon_3_manifest.json"
        )
        .to_string();

        // The empty property for addons
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_1"
                ),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_2"
                ),
                ext2_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_3"
                ),
                ext3_manifest,
                empty_property.clone(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            );
            assert!(inject_ret.await.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(
                &graphs_cache,
                "default_with_app_uri",
            )
            .unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs/connections/add",
                web::post().to(add_graph_connection_endpoint),
            ),
        )
        .await;

        // Add a connection between existing nodes in the default graph.
        // Use "http://example.com:8000" for both src_app and dest_app to match the test data.
        let request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "test_cmd".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Print the status and body for debugging.
        let status = resp.status();
        println!("Response status: {status:?}");
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert!(status.is_success());

        let response: ApiResponse<AddGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();

        assert!(response.data.success);

        // Define expected property.json content after adding the connection.
        let expected_property_json_str = include_str!(
            "../../../../test_data/\
             expected_json__test_add_graph_connection_success.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_add_graph_connection_success_2() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str = include_str!(
            "../../../../test_data/initial_property_add_connection_2.json"
        )
        .to_string();

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings.
        let ext1_manifest = r#"{
            "type": "extension",
            "name": "aio_http_server_python",
            "version": "0.1.0"
        }"#
        .to_string();

        let ext2_manifest = r#"{
            "type": "extension",
            "name": "simple_echo_cpp",
            "version": "0.1.0"
        }"#
        .to_string();

        let ext3_manifest = r#"{
            "type": "extension",
            "name": "mock_extension_1",
            "version": "0.1.0"
        }"#
        .to_string();

        let ext4_manifest = r#"{
          "type": "extension",
          "name": "mock_extension_2",
          "version": "0.1.0"
        }"#
        .to_string();

        // The empty property for addons
        let empty_property = r#"{}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_1"
                ),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_2"
                ),
                ext2_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_3"
                ),
                ext3_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_4"
                ),
                ext4_manifest,
                empty_property.clone(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            );
            assert!(inject_ret.await.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) =
                graphs_cache_find_by_name(&graphs_cache, "default").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs/connections/add",
                web::post().to(add_graph_connection_endpoint),
            ),
        )
        .await;

        // Add a connection between existing nodes in the default graph.
        let request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: None,
            src_extension: "aio_http_server_python".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "c1".to_string(),
            dest_app: None,
            dest_extension: "simple_echo_cpp".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Print the status and body for debugging.
        let status = resp.status();
        println!("Response status: {status:?}");
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert!(status.is_success());

        let response: ApiResponse<AddGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();

        assert!(response.data.success);

        // Define expected property.json content after adding the connection.
        let expected_property_json_str = include_str!(
            "../../../../test_data/expected_property_add_connection_2.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_add_graph_connection_invalid_graph() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        let all_pkgs_json_str = vec![(
            test_dir.clone(),
            include_str!("../../../../test_data/app_manifest.json").to_string(),
            include_str!("../../../../test_data/app_property.json").to_string(),
        )];

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(
            &property_path,
            include_str!("../../../../test_data/app_property.json"),
        )
        .unwrap();

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json_str,
            );
            assert!(inject_ret.await.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs/connections/add",
                web::post().to(add_graph_connection_endpoint),
            ),
        )
        .await;

        // Try to add a connection to a non-existent graph.
        let request_payload = AddGraphConnectionRequestPayload {
            graph_id: Uuid::new_v4(),
            src_app: None,
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "test_cmd".to_string(),
            dest_app: None,
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert_eq!(resp.status(), 404);
    }

    #[actix_web::test]
    async fn test_add_graph_connection_preserves_order() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings.
        let ext1_manifest = include_str!(
            "../../../../test_data/extension_addon_1_manifest.json"
        )
        .to_string();

        let ext2_manifest = include_str!(
            "../../../../test_data/extension_addon_2_manifest.json"
        )
        .to_string();

        let ext3_manifest = include_str!(
            "../../../../test_data/extension_addon_3_manifest.json"
        )
        .to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_1"
                ),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_2"
                ),
                ext2_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_3"
                ),
                ext3_manifest,
                empty_property.clone(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            );
            assert!(inject_ret.await.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(
                &graphs_cache,
                "default_with_app_uri",
            )
            .unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state_arc = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state_arc.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                ),
        )
        .await;

        // Add first connection.
        let request_payload1 = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "test_cmd1".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req1 = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload1)
            .to_request();
        let resp1 = test::call_service(&app, req1).await;

        assert!(resp1.status().is_success());

        // Add second connection to create a sequence.
        let request_payload2 = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "test_cmd2".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_3".to_string(),
            msg_conversion: None,
        };

        let req2 = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload2)
            .to_request();
        let resp2 = test::call_service(&app, req2).await;

        assert!(resp2.status().is_success());

        // Define expected property.json content after adding both connections.
        let expected_property_json_str = include_str!(
            "../../../../test_data/\
             expected_json__test_add_graph_connection_preserves_order.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_add_graph_connection_file_comparison() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings.
        let ext1_manifest = include_str!(
            "../../../../test_data/extension_addon_1_manifest.json"
        )
        .to_string();

        let ext2_manifest = include_str!(
            "../../../../test_data/extension_addon_2_manifest.json"
        )
        .to_string();

        let ext3_manifest = include_str!(
            "../../../../test_data/extension_addon_3_manifest.json"
        )
        .to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_1"
                ),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_2"
                ),
                ext2_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_3"
                ),
                ext3_manifest,
                empty_property.clone(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            );
            assert!(inject_ret.await.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(
                &graphs_cache,
                "default_with_app_uri",
            )
            .unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state_arc = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state_arc.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                ),
        )
        .await;

        // Add first connection.
        let request_payload1 = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "test_cmd1".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req1 = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload1)
            .to_request();
        let resp1 = test::call_service(&app, req1).await;

        assert!(resp1.status().is_success());

        // Add second connection to create a sequence.
        let request_payload2 = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "test_cmd2".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_3".to_string(),
            msg_conversion: None,
        };

        let req2 = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload2)
            .to_request();
        let resp2 = test::call_service(&app, req2).await;

        assert!(resp2.status().is_success());

        // Define expected property.json content after adding both connections.
        let expected_property_json_str = include_str!(
            "../../../../test_data/\
             expected_json__test_add_graph_connection_file_comparison.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_add_graph_connection_data_type() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings.
        let ext1_manifest = include_str!(
            "../../../../test_data/extension_addon_1_manifest.json"
        )
        .to_string();

        let ext2_manifest = include_str!(
            "../../../../test_data/extension_addon_2_manifest.json"
        )
        .to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_1"
                ),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_2"
                ),
                ext2_manifest,
                empty_property,
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            );
            assert!(inject_ret.await.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(
                &graphs_cache,
                "default_with_app_uri",
            )
            .unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state_arc = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state_arc.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                ),
        )
        .await;

        // Add a DATA type connection between extensions.
        let request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Data,
            msg_name: "test_data".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("{body_str}");

        let response: ApiResponse<AddGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert!(response.data.success);

        // Define expected property.json content after adding the connection.
        let expected_property_json_str = include_str!(
            "../../../../test_data/\
             expected_json__test_add_graph_connection_data_type.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_add_graph_connection_frame_types() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings.
        let ext1_manifest = include_str!(
            "../../../../test_data/extension_addon_1_manifest.json"
        )
        .to_string();

        let ext2_manifest = include_str!(
            "../../../../test_data/extension_addon_2_manifest.json"
        )
        .to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_1"
                ),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_2"
                ),
                ext2_manifest,
                empty_property,
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            );
            assert!(inject_ret.await.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(
                &graphs_cache,
                "default_with_app_uri",
            )
            .unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state_arc = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state_arc.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                ),
        )
        .await;

        // First add an AUDIO_FRAME type connection.
        let audio_request = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::AudioFrame,
            msg_name: "audio_stream".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(audio_request)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        // Then add a VIDEO_FRAME type connection.
        let video_request = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::VideoFrame,
            msg_name: "video_stream".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(video_request)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        // Define expected property.json content after adding both connections.
        let expected_property_json_str = include_str!(
            "../../../../test_data/\
             expected_json__test_add_graph_connection_frame_types.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_add_multiple_connections_preservation_order() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create three extension addon manifest strings.
        let ext1_manifest = include_str!(
            "../../../../test_data/extension_addon_1_manifest.json"
        )
        .to_string();

        let ext2_manifest = include_str!(
            "../../../../test_data/extension_addon_2_manifest.json"
        )
        .to_string();

        let ext3_manifest = include_str!(
            "../../../../test_data/extension_addon_3_manifest.json"
        )
        .to_string();
        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_1"
                ),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_2"
                ),
                ext2_manifest,
                empty_property.clone(),
            ),
            (
                format!(
                    "{}{}",
                    test_dir.clone(),
                    "/ten_packages/extension/extension_addon_3"
                ),
                ext3_manifest,
                empty_property,
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            );
            assert!(inject_ret.await.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(
                &graphs_cache,
                "default_with_app_uri",
            )
            .unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state_arc = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state_arc.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                ),
        )
        .await;

        // Add first command.
        let cmd1_request = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "cmd_1".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(cmd1_request)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        // Add second command.
        let cmd2_request = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "cmd_2".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_3".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(cmd2_request)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        // Add third command.
        let cmd3_request = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: Some("http://example.com:8000".to_string()),
            src_extension: "extension_1".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "cmd_3".to_string(),
            dest_app: Some("http://example.com:8000".to_string()),
            dest_extension: "extension_2".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(cmd3_request)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        // Define expected property.json content after adding all three
        // connections.
        let expected_property_json_str = include_str!(
            "../../../../test_data/\
             expected_json__test_add_multiple_connections_preservation_order.\
             json"
        );

        // Read the actual property.json file generated during the test.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        let actual_property = std::fs::read_to_string(property_path).unwrap();

        // Normalize both JSON strings to handle formatting differences.
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_property).unwrap();

        // Compare the normalized JSON values.
        assert_eq!(
            expected_value,
            actual_value,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_value).unwrap(),
            serde_json::to_string_pretty(&actual_value).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_add_graph_connection_to_extension_with_interface() {
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

        // Create a temporary directory for our test to store the generated
        // property.json.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Copy the test directory to the temporary directory.
        let test_data_dir = std::path::Path::new("tests")
            .join("test_data")
            .join("graph_add_connection_to_extension_with_interface");

        copy_folder_recursively(
            &test_data_dir.to_str().unwrap().to_string(),
            &test_dir,
        )
        .unwrap();

        // Get the new created test directory.
        let test_data_dir = std::path::Path::new(&test_dir)
            .join("graph_add_connection_to_extension_with_interface");

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &test_data_dir.to_str().unwrap().to_string(),
            )
            .await
            .unwrap();
        }

        {
            let pkgs_cache = designer_state.pkgs_cache.read().await;
            let graphs_cache = designer_state.graphs_cache.read().await;
            println!("Packages in cache: {pkgs_cache:?}");
            println!("Graphs in cache: {graphs_cache:?}");
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) =
                graphs_cache_find_by_name(&graphs_cache, "default").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs/connections/add",
                web::post().to(add_graph_connection_endpoint),
            ),
        )
        .await;

        // Add connections between existing nodes in the default graph.

        // Add a connection between "ext_b" and "ext_a" with "hello" cmd.
        let request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: None,
            src_extension: "ext_b".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "hello".to_string(),
            dest_app: None,
            dest_extension: "ext_a".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        let status = resp.status();
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        assert!(status.is_success(), "status: {status:?}, body: {body_str}");

        let response: ApiResponse<AddGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert!(response.data.success);

        // Add a connection between "ext_a" and "ext_b" with "cmd_out_b" cmd.
        let request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: None,
            src_extension: "ext_a".to_string(),
            msg_type: MsgType::Cmd,
            msg_name: "cmd_out_b".to_string(),
            dest_app: None,
            dest_extension: "ext_b".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        let status = resp.status();
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        assert!(status.is_success(), "status: {status:?}, body: {body_str}");

        let response: ApiResponse<AddGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert!(response.data.success);

        // Add a connection between "ext_b" and "ext_a" with "data" data.
        let request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src_app: None,
            src_extension: "ext_b".to_string(),
            msg_type: MsgType::Data,
            msg_name: "data".to_string(),
            dest_app: None,
            dest_extension: "ext_a".to_string(),
            msg_conversion: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        let status = resp.status();
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        assert!(status.is_success(), "status: {status:?}, body: {body_str}");

        let response: ApiResponse<AddGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert!(response.data.success);
    }
}
