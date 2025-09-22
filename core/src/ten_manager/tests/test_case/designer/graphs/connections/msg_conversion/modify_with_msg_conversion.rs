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
            graphs::connections::{
                add::{add_graph_connection_endpoint, AddGraphConnectionRequestPayload},
                msg_conversion::update::{
                    update_graph_connection_msg_conversion_endpoint,
                    UpdateGraphConnectionMsgConversionRequestPayload,
                    UpdateGraphConnectionMsgConversionResponsePayload,
                },
            },
            response::ApiResponse,
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        graph::graphs_cache_find_by_name,
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use ten_rust::{
        graph::{
            connection::GraphLoc,
            msg_conversion::{
                MsgAndResultConversion, MsgConversion, MsgConversionMode, MsgConversionRule,
                MsgConversionRules, MsgConversionType,
            },
            node::GraphNodeType,
        },
        pkg_info::{constants::PROPERTY_JSON_FILENAME, message::MsgType},
    };

    use crate::test_case::common::mock::inject_all_pkgs_for_mock;

    #[actix_web::test]
    async fn test_update_graph_connection_msg_conversion_1() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings
        let ext1_manifest_json_str =
            include_str!("../../../../../test_data/extension_1_manifest.json").to_string();

        let ext2_manifest_json_str =
            include_str!("../../../../../test_data/extension_2_manifest.json").to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_1"),
                ext1_manifest_json_str,
                empty_property.clone(),
            ),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_2"),
                ext2_manifest_json_str,
                empty_property.clone(),
            ),
        ];

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

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                )
                .route(
                    "/api/designer/v1/graphs/connections/msg_conversion/update",
                    web::post().to(update_graph_connection_msg_conversion_endpoint),
                ),
        )
        .await;

        // First, add a connection with initial message conversion.
        let initial_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        // Initial fixed value rule.
                        MsgConversionRule {
                            path: "initial_property".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("initial_value")),
                        },
                    ],
                    keep_original: Some(true),
                },
            }),
            result: None,
        };

        // Add a connection between existing nodes in the default graph.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_2".to_string(),
        )
        .unwrap();

        let add_request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src: src.clone(),
            dest: dest.clone(),
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(initial_msg_conversion),
        };

        // Add the initial connection.
        let add_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(add_request_payload)
            .to_request();
        let add_resp = test::call_service(&app, add_req).await;

        assert!(add_resp.status().is_success(), "Failed to add initial connection");

        // Create updated message conversion rules.
        let updated_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        // Updated fixed value rule.
                        MsgConversionRule {
                            path: "updated_property".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("updated_value")),
                        },
                        // New from original rule.
                        MsgConversionRule {
                            path: "new_copied_property".to_string(),
                            conversion_mode: MsgConversionMode::FromOriginal,
                            original_path: Some("original_source".to_string()),
                            value: None,
                        },
                    ],
                    keep_original: Some(false),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "result_property".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("original_result".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        // Now update the connection's message conversion.
        let update_request_payload = UpdateGraphConnectionMsgConversionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(updated_msg_conversion),
        };

        let update_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/msg_conversion/update")
            .set_json(update_request_payload)
            .to_request();
        let update_resp = test::call_service(&app, update_req).await;

        // Print the status and body for debugging.
        let status = update_resp.status();
        println!("Response status: {status:?}");
        let body = test::read_body(update_resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert!(status.is_success());

        let response: ApiResponse<UpdateGraphConnectionMsgConversionResponsePayload> =
            serde_json::from_str(body_str).unwrap();

        assert!(response.data.success);

        // Define expected property.json content after updating the message
        // conversion.
        let expected_property_json_str = include_str!(
            "../../../../../test_data/expected_json__connection_with_updated_msg_conversion.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
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
    async fn test_update_graph_connection_msg_conversion_2() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../../test_data/app_property_2.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings
        let ext1_manifest_json_str =
            include_str!("../../../../../test_data/extension_1_manifest.json").to_string();

        let ext2_manifest_json_str =
            include_str!("../../../../../test_data/extension_2_manifest_3.json").to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_1"),
                ext1_manifest_json_str,
                empty_property.clone(),
            ),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_2"),
                ext2_manifest_json_str,
                empty_property.clone(),
            ),
        ];

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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/msg_conversion/update",
                web::post().to(update_graph_connection_msg_conversion_endpoint),
            ))
            .await;

        // Create updated message conversion rules.
        let updated_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        MsgConversionRule {
                            path: "aaa".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("updated_value")),
                        },
                        MsgConversionRule {
                            path: "new_copied_property".to_string(),
                            conversion_mode: MsgConversionMode::FromOriginal,
                            original_path: Some("original_source".to_string()),
                            value: None,
                        },
                    ],
                    keep_original: Some(false),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "result_property".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("original_result".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        // Now update the connection's message conversion.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_2".to_string(),
        )
        .unwrap();
        let update_request_payload = UpdateGraphConnectionMsgConversionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["change_name".to_string()],
            msg_conversion: Some(updated_msg_conversion),
        };

        let update_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/msg_conversion/update")
            .set_json(update_request_payload)
            .to_request();
        let update_resp = test::call_service(&app, update_req).await;

        // Print the status and body for debugging.
        let status = update_resp.status();
        println!("Response status: {status:?}");
        let body = test::read_body(update_resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert!(status.is_success());

        let response: ApiResponse<UpdateGraphConnectionMsgConversionResponsePayload> =
            serde_json::from_str(body_str).unwrap();

        assert!(response.data.success);

        // Define expected property.json content after updating the message
        // conversion.
        let expected_property_json_str = include_str!(
            "../../../../../test_data/expected_json__connection_with_updated_msg_conversion_1.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
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
    async fn test_update_graph_connection_msg_conversion_3() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../../test_data/app_property_3.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings
        let ext1_manifest_json_str =
            include_str!("../../../../../test_data/extension_1_manifest.json").to_string();

        let ext2_manifest_json_str =
            include_str!("../../../../../test_data/extension_2_manifest_3.json").to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_1"),
                ext1_manifest_json_str,
                empty_property.clone(),
            ),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_2"),
                ext2_manifest_json_str,
                empty_property.clone(),
            ),
        ];

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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/msg_conversion/update",
                web::post().to(update_graph_connection_msg_conversion_endpoint),
            ))
            .await;

        // Create updated message conversion rules.
        let updated_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        MsgConversionRule {
                            path: "ten.name".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("change_name")),
                        },
                        MsgConversionRule {
                            path: "aaa".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("updated_value")),
                        },
                        MsgConversionRule {
                            path: "new_copied_property".to_string(),
                            conversion_mode: MsgConversionMode::FromOriginal,
                            original_path: Some("original_source".to_string()),
                            value: None,
                        },
                    ],
                    keep_original: Some(false),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "result_property".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("original_result".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        // Now update the connection's message conversion.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_2".to_string(),
        )
        .unwrap();
        let update_request_payload = UpdateGraphConnectionMsgConversionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(updated_msg_conversion),
        };

        let update_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/msg_conversion/update")
            .set_json(update_request_payload)
            .to_request();
        let update_resp = test::call_service(&app, update_req).await;

        // Print the status and body for debugging.
        let status = update_resp.status();
        println!("Response status: {status:?}");
        let body = test::read_body(update_resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert!(status.is_success());

        let response: ApiResponse<UpdateGraphConnectionMsgConversionResponsePayload> =
            serde_json::from_str(body_str).unwrap();

        assert!(response.data.success);

        // Define expected property.json content after updating the message
        // conversion.
        let expected_property_json_str = include_str!(
            "../../../../../test_data/expected_json__connection_with_updated_msg_conversion_2.json"
        );

        // Read the actual property.json file generated during the test.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
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
    async fn test_update_graph_connection_msg_conversion_schema_failure_1() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings
        let ext1_manifest_json_str =
            include_str!("../../../../../test_data/extension_1_manifest.json").to_string();

        let ext2_manifest_json_str =
            include_str!("../../../../../test_data/extension_2_manifest.json").to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_1"),
                ext1_manifest_json_str,
                empty_property.clone(),
            ),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_2"),
                ext2_manifest_json_str,
                empty_property.clone(),
            ),
        ];

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

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                )
                .route(
                    "/api/designer/v1/graphs/connections/msg_conversion/update",
                    web::post().to(update_graph_connection_msg_conversion_endpoint),
                ),
        )
        .await;

        // First, add a connection with initial message conversion.
        let initial_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        // Initial fixed value rule.
                        MsgConversionRule {
                            path: "initial_property".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("initial_value")),
                        },
                    ],
                    keep_original: Some(true),
                },
            }),
            result: None,
        };

        // Add a connection between existing nodes in the default graph.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_2".to_string(),
        )
        .unwrap();

        let add_request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src: src.clone(),
            dest: dest.clone(),
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(initial_msg_conversion),
        };

        // Add the initial connection.
        let add_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(add_request_payload)
            .to_request();
        let add_resp = test::call_service(&app, add_req).await;

        assert!(add_resp.status().is_success(), "Failed to add initial connection");

        // Create updated message conversion rules.
        let updated_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        // Updated fixed value rule.
                        MsgConversionRule {
                            path: "updated_property".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!(3)),
                        },
                        // New from original rule.
                        MsgConversionRule {
                            path: "new_copied_property".to_string(),
                            conversion_mode: MsgConversionMode::FromOriginal,
                            original_path: Some("original_source".to_string()),
                            value: None,
                        },
                    ],
                    keep_original: Some(false),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "result_property".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("original_result".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        // Now update the connection's message conversion.
        let update_request_payload = UpdateGraphConnectionMsgConversionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(updated_msg_conversion),
        };

        let update_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/msg_conversion/update")
            .set_json(update_request_payload)
            .to_request();
        let update_resp = test::call_service(&app, update_req).await;

        // Print the status and body for debugging.
        let status = update_resp.status();
        println!("Response status: {status:?}");
        let body = test::read_body(update_resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert!(!status.is_success());
    }

    #[actix_web::test]
    async fn test_update_graph_connection_msg_conversion_schema_failure_2() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings
        let ext1_manifest_json_str =
            include_str!("../../../../../test_data/extension_1_manifest.json").to_string();

        let ext2_manifest_json_str =
            include_str!("../../../../../test_data/extension_2_manifest.json").to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_1"),
                ext1_manifest_json_str,
                empty_property.clone(),
            ),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_2"),
                ext2_manifest_json_str,
                empty_property.clone(),
            ),
        ];

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

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                )
                .route(
                    "/api/designer/v1/graphs/connections/msg_conversion/update",
                    web::post().to(update_graph_connection_msg_conversion_endpoint),
                ),
        )
        .await;

        // First, add a connection with initial message conversion.
        let initial_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        // Initial fixed value rule.
                        MsgConversionRule {
                            path: "initial_property".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("initial_value")),
                        },
                    ],
                    keep_original: Some(true),
                },
            }),
            result: None,
        };

        // Add a connection between existing nodes in the default graph.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_2".to_string(),
        )
        .unwrap();
        let add_request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src: src.clone(),
            dest: dest.clone(),
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(initial_msg_conversion),
        };

        // Add the initial connection.
        let add_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(add_request_payload)
            .to_request();
        let add_resp = test::call_service(&app, add_req).await;

        assert!(add_resp.status().is_success(), "Failed to add initial connection");

        // Create updated message conversion rules.
        let updated_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        MsgConversionRule {
                            path: "ten.name".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("change_name")),
                        },
                        // Updated fixed value rule.
                        MsgConversionRule {
                            path: "aaa".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!(3)),
                        },
                        // New from original rule.
                        MsgConversionRule {
                            path: "new_copied_property".to_string(),
                            conversion_mode: MsgConversionMode::FromOriginal,
                            original_path: Some("original_source".to_string()),
                            value: None,
                        },
                    ],
                    keep_original: Some(false),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "result_property".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("original_result".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        // Now update the connection's message conversion.
        let update_request_payload = UpdateGraphConnectionMsgConversionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(updated_msg_conversion),
        };

        let update_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/msg_conversion/update")
            .set_json(update_request_payload)
            .to_request();
        let update_resp = test::call_service(&app, update_req).await;

        // Print the status and body for debugging.
        let status = update_resp.status();
        println!("Response status: {status:?}");

        let body = test::read_body(update_resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert!(!status.is_success());
    }

    #[actix_web::test]
    async fn test_update_graph_connection_msg_conversion_schema_failure_3() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../../test_data/app_property_3.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings
        let ext1_manifest_json_str =
            include_str!("../../../../../test_data/extension_1_manifest.json").to_string();

        let ext2_manifest_json_str =
            include_str!("../../../../../test_data/extension_2_manifest_3.json").to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_1"),
                ext1_manifest_json_str,
                empty_property.clone(),
            ),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_2"),
                ext2_manifest_json_str,
                empty_property.clone(),
            ),
        ];

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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/msg_conversion/update",
                web::post().to(update_graph_connection_msg_conversion_endpoint),
            ))
            .await;

        // Create updated message conversion rules.
        let updated_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        MsgConversionRule {
                            path: "ten.name".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!("change_name")),
                        },
                        MsgConversionRule {
                            path: "aaa".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::json!(3)),
                        },
                        MsgConversionRule {
                            path: "new_copied_property".to_string(),
                            conversion_mode: MsgConversionMode::FromOriginal,
                            original_path: Some("original_source".to_string()),
                            value: None,
                        },
                    ],
                    keep_original: Some(false),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "result_property".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("original_result".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        // Now update the connection's message conversion.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_2".to_string(),
        )
        .unwrap();
        let update_request_payload = UpdateGraphConnectionMsgConversionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_for_update".to_string()],
            msg_conversion: Some(updated_msg_conversion),
        };

        let update_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/msg_conversion/update")
            .set_json(update_request_payload)
            .to_request();
        let update_resp = test::call_service(&app, update_req).await;

        // Print the status and body for debugging.
        let status = update_resp.status();
        println!("Response status: {status:?}");

        assert!(!status.is_success());

        let body = test::read_body(update_resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        assert_eq!(
            body_str,
            r#"{"status":"fail","message":"Failed to validate connection schema: { .aaa: type is incompatible, source is [uint64], but target is [string] }"}"#
        );
    }

    #[actix_web::test]
    async fn test_update_graph_connection_remove_msg_conversion() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Create a temporary directory for our test.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Load both the app package JSON and extension addon package JSONs.
        let app_manifest_json_str =
            include_str!("../../../../../test_data/app_manifest.json").to_string();
        let app_property_json_str =
            include_str!("../../../../../test_data/app_property.json").to_string();

        // Create the property.json file in the temporary directory.
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, &app_property_json_str).unwrap();

        // Create extension addon manifest strings.
        let ext1_manifest =
            include_str!("../../../../../test_data/extension_1_simple_manifest.json").to_string();

        let ext2_manifest =
            include_str!("../../../../../test_data/extension_2_simple_manifest.json").to_string();

        // The empty property for addons.
        let empty_property = r#"{"ten":{}}"#.to_string();

        let all_pkgs_json = vec![
            (test_dir.clone(), app_manifest_json_str, app_property_json_str),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_addon_1"),
                ext1_manifest,
                empty_property.clone(),
            ),
            (
                format!("{}{}", test_dir.clone(), "/ten_packages/extension/extension_addon_2"),
                ext2_manifest,
                empty_property.clone(),
            ),
        ];

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

        // First, add a connection with initial message conversion.
        let initial_msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "to_be_removed".to_string(),
                        conversion_mode: MsgConversionMode::FixedValue,
                        original_path: None,
                        value: Some(serde_json::json!("value_to_remove")),
                    }],
                    keep_original: Some(true),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "result_to_remove".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("original_result".to_string()),
                        value: None,
                    }],
                    keep_original: Some(true),
                },
            }),
        };

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route(
                    "/api/designer/v1/graphs/connections/add",
                    web::post().to(add_graph_connection_endpoint),
                )
                .route(
                    "/api/designer/v1/graphs/connections/msg_conversion/update",
                    web::post().to(update_graph_connection_msg_conversion_endpoint),
                ),
        )
        .await;

        // Add a connection with msg_conversion.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "extension_2".to_string(),
        )
        .unwrap();
        let add_request_payload = AddGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src: src.clone(),
            dest: dest.clone(),
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_remove_conversion".to_string()],
            msg_conversion: Some(initial_msg_conversion),
        };

        // Add the initial connection.
        let add_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/add")
            .set_json(add_request_payload)
            .to_request();
        let add_resp = test::call_service(&app, add_req).await;

        assert!(add_resp.status().is_success(), "Failed to add initial connection");

        // Now update the connection to remove the message conversion.
        let update_request_payload = UpdateGraphConnectionMsgConversionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_cmd_remove_conversion".to_string()],
            // Set to None to remove the msg_conversion.
            msg_conversion: None,
        };

        let update_req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/msg_conversion/update")
            .set_json(update_request_payload)
            .to_request();
        let update_resp = test::call_service(&app, update_req).await;

        assert!(update_resp.status().is_success());

        let body = test::read_body(update_resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        let response: ApiResponse<UpdateGraphConnectionMsgConversionResponsePayload> =
            serde_json::from_str(body_str).unwrap();

        assert!(response.data.success);

        // Define expected property.json content after removing the message
        // conversion.
        let expected_property_json_str = include_str!(
            "../../../../../test_data/expected_json__connection_with_removed_msg_conversion.json"
        );

        // Read the actual property.json file generated during the test
        let property_path = std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
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
