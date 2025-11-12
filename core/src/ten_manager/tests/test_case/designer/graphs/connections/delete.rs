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
            graphs::connections::delete::{
                delete_graph_connection_endpoint, DeleteGraphConnectionRequestPayload,
                DeleteGraphConnectionResponsePayload,
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
        graph::{connection::GraphLoc, node::GraphNodeType},
        pkg_info::{
            constants::{MANIFEST_JSON_FILENAME, PROPERTY_JSON_FILENAME},
            message::MsgType,
        },
    };
    use uuid::Uuid;

    use crate::test_case::common::mock::{
        inject_all_pkgs_for_mock, inject_all_standard_pkgs_for_mock,
    };

    #[actix_web::test]
    async fn test_delete_graph_connection_invalid_graph() {
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
            "/api/designer/v1/graphs/connections/delete",
            web::post().to(delete_graph_connection_endpoint),
        ))
        .await;

        // Try to delete a connection from a non-existent graph.
        let src = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Extension,
            "source_extension".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Extension,
            "destination_extension".to_string(),
        )
        .unwrap();
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: Uuid::new_v4(),
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["test_message".to_string()],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
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
    async fn test_delete_graph_connection_nonexistent_connection() {
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
            "/api/designer/v1/graphs/connections/delete",
            web::post().to(delete_graph_connection_endpoint),
        ))
        .await;

        // Try to delete a non-existent connection.
        let src = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Extension,
            "nonexistent_extension".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Extension,
            "nonexistent_destination".to_string(),
        )
        .unwrap();

        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["nonexistent_message".to_string()],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should fail with a 400 Bad Request for nonexistent connections.
        assert_eq!(resp.status(), 400);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Fail);
    }

    #[actix_web::test]
    async fn test_delete_graph_connection_success() {
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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/delete",
                web::post().to(delete_graph_connection_endpoint),
            ))
            .await;

        // Delete a connection from the default_with_app_uri graph.
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
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["hello_world".to_string()],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // Verify the connection was actually removed from the data.
        let graphs_cache = designer_state.graphs_cache.read().await;
        if let Some(predefined_graph) = graphs_cache_find_by_id(&graphs_cache, &graph_id_clone) {
            // Check if the connection is gone.
            let connection_exists =
                predefined_graph.graph.connections().as_ref().is_some_and(|connections| {
                    connections.iter().any(|conn| {
                        conn.loc.extension == Some("extension_1".to_string())
                            && conn
                                .loc
                                .app
                                .as_ref()
                                .is_some_and(|app| app == "http://example.com:8000")
                            && conn.cmd.as_ref().is_some_and(|cmds| {
                                cmds.iter().any(|cmd| cmd.name.as_deref() == Some("hello_world"))
                            })
                    })
                });
            assert!(!connection_exists, "Connection should have been deleted");
        } else {
            panic!("Graph 'default_with_app_uri' not found");
        }

        // Read the updated property.json file.
        let updated_property_content = std::fs::read_to_string(&property_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_content).unwrap();

        // Create the expected property JSON, which is the same as input but
        // with the connection removed.
        let expected_property_json_str =
            include_str!("../../../../test_data/expected_property_delete_connection.json");

        // Parse the expected property JSON.
        let expected_property: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();

        assert_eq!(
            updated_property,
            expected_property,
            "Property file doesn't match expected content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_property).unwrap(),
            serde_json::to_string_pretty(&updated_property).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_delete_graph_connection_subgraph_to_subgraph_success() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str = include_str!(
            "../../../../test_data/graph_add_connection_subgraph_to_subgraph/expected_property.\
             json"
        );
        let input_manifest_json_str = include_str!(
            "../../../../test_data/graph_add_connection_subgraph_to_subgraph/manifest.json"
        );

        // println!("Property JSON: {}", input_property_json_str);
        // println!("Manifest JSON: {}", input_manifest_json_str);

        // Write input files to temp directory.
        let property_path = std::path::Path::new(&temp_dir_path).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, input_property_json_str).unwrap();

        let manifest_path = std::path::Path::new(&temp_dir_path).join(MANIFEST_JSON_FILENAME);
        std::fs::write(&manifest_path, input_manifest_json_str).unwrap();

        // Create graphs directory and write required graph files
        let graphs_dir = std::path::Path::new(&temp_dir_path).join("graphs");
        std::fs::create_dir_all(&graphs_dir).unwrap();

        let test_graph_1_content = include_str!(
            "../../../../test_data/graph_add_connection_subgraph_to_subgraph/graphs/test_graph_1.\
             json"
        );
        let test_graph_2_content = include_str!(
            "../../../../test_data/graph_add_connection_subgraph_to_subgraph/graphs/test_graph_2.\
             json"
        );

        std::fs::write(graphs_dir.join("test_graph_1.json"), test_graph_1_content).unwrap();
        std::fs::write(graphs_dir.join("test_graph_2.json"), test_graph_2_content).unwrap();

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
            if let Err(e) = &inject_ret {
                eprintln!("inject_all_pkgs_for_mock failed: {e}");
            }
            assert!(inject_ret.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(&graphs_cache, "default").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/delete",
                web::post().to(delete_graph_connection_endpoint),
            ))
            .await;

        // Delete a connection from the default graph.
        let src = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Subgraph,
            "subgraph_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Subgraph,
            "subgraph_2".to_string(),
        )
        .unwrap();
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["C".to_string()],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // Verify the connection was actually removed from the data.
        let graphs_cache = designer_state.graphs_cache.read().await;
        if let Some(predefined_graph) = graphs_cache_find_by_id(&graphs_cache, &graph_id_clone) {
            // Check if the connection is gone.
            let connection_exists =
                predefined_graph.graph.connections().as_ref().is_some_and(|connections| {
                    connections.iter().any(|conn| {
                        conn.loc.subgraph == Some("subgraph_1".to_string())
                            && conn
                                .loc
                                .app
                                .as_ref()
                                .is_some_and(|app| app == "http://example.com:8000")
                            && conn.cmd.as_ref().is_some_and(|cmds| {
                                cmds.iter().any(|cmd| cmd.name.as_deref() == Some("hello_world"))
                            })
                    })
                });
            assert!(!connection_exists, "Connection should have been deleted");
        } else {
            panic!("Graph 'default' not found");
        }

        // Read the updated property.json file.
        let updated_property_content = std::fs::read_to_string(&property_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_content).unwrap();

        // Create the expected property JSON, which is the same as input but
        // with the connection removed.
        let expected_property_json_str = include_str!(
            "../../../../test_data/graph_add_connection_subgraph_to_subgraph/property.json"
        );

        // Parse the expected property JSON.
        let expected_property: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();

        assert_eq!(
            updated_property,
            expected_property,
            "Property file doesn't match expected content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_property).unwrap(),
            serde_json::to_string_pretty(&updated_property).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_delete_graph_connection_between_selectors_success() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str = include_str!(
            "../../../../test_data/graph_add_connection_for_selector/expected_property.json"
        );
        let input_manifest_json_str =
            include_str!("../../../../test_data/graph_add_connection_for_selector/manifest.json");

        //println!("Property JSON: {}", input_property_json_str);
        //println!("Manifest JSON: {}", input_manifest_json_str);

        // Write input files to temp directory.
        let property_path = std::path::Path::new(&temp_dir_path).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, input_property_json_str).unwrap();

        let manifest_path = std::path::Path::new(&temp_dir_path).join(MANIFEST_JSON_FILENAME);
        std::fs::write(&manifest_path, input_manifest_json_str).unwrap();

        // Create graphs directory and write required graph files
        let graphs_dir = std::path::Path::new(&temp_dir_path).join("graphs");
        std::fs::create_dir_all(&graphs_dir).unwrap();

        // Initialize test state.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        println!("temp_dir_path: {temp_dir_path}");
        // Inject the test app into the mock.
        let all_pkgs_json = vec![(
            temp_dir_path.clone(),
            std::fs::read_to_string(&manifest_path).unwrap(),
            std::fs::read_to_string(&property_path).unwrap(),
        )];
        println!("all_pkgs_json: {all_pkgs_json:?}");
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json).await;
            if let Err(e) = &inject_ret {
                eprintln!("inject_all_pkgs_for_mock failed: {e}");
            }
            assert!(inject_ret.is_ok());
        }

        let graph_id_clone;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let (graph_id, _) = graphs_cache_find_by_name(&graphs_cache, "default").unwrap();

            graph_id_clone = *graph_id;
        }

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/delete",
                web::post().to(delete_graph_connection_endpoint),
            ))
            .await;

        // Delete all connections from the default graph.
        // 1. delete to_selector connection
        let src = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Extension,
            "ext_3".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Selector,
            "selector_for_ext_1_and_2".to_string(),
        )
        .unwrap();
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["to_selector".to_string()],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // 2. delete from_selector connection
        let src = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Selector,
            "selector_for_ext_1_and_2_and_3".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Extension,
            "ext_4".to_string(),
        )
        .unwrap();
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["from_selector".to_string()],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // 3. from_selector_to_selector connection
        let src = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Selector,
            "selector_for_ext_1_and_2_and_3".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            None,
            GraphNodeType::Selector,
            "selector_for_ext_1_or_3".to_string(),
        )
        .unwrap();
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec!["from_selector_to_selector".to_string()],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // Verify all connections were actually removed from the data.
        let graphs_cache = designer_state.graphs_cache.read().await;
        if let Some(predefined_graph) = graphs_cache_find_by_id(&graphs_cache, &graph_id_clone) {
            // Check if all connections are gone.
            let connection_gone = predefined_graph.graph.connections().as_ref().is_none();
            assert!(connection_gone, "All connections should have been deleted");
        } else {
            panic!("Graph 'default' not found");
        }

        // Read the updated property.json file.
        let updated_property_content = std::fs::read_to_string(&property_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_content).unwrap();

        // Create the expected property JSON, which is the same as input but
        // with the connection removed.
        let expected_property_json_str =
            include_str!("../../../../test_data/graph_add_connection_for_selector/property.json");

        // Parse the expected property JSON.
        let expected_property: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();

        assert_eq!(
            updated_property,
            expected_property,
            "Property file doesn't match expected content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_property).unwrap(),
            serde_json::to_string_pretty(&updated_property).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_delete_graph_connection_multiple_msg_names_success() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str =
            include_str!("../../../../test_data/app_property_multi_names_flow.json");
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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/delete",
                web::post().to(delete_graph_connection_endpoint),
            ))
            .await;

        // Delete a connection from the default_with_app_uri graph.
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
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec![
                "multi_name_1".to_string(),
                "multi_name_2".to_string(),
                "multi_name_3".to_string(),
            ],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // Verify the connection was actually removed from the data.
        let graphs_cache = designer_state.graphs_cache.read().await;
        if let Some(predefined_graph) = graphs_cache_find_by_id(&graphs_cache, &graph_id_clone) {
            // Check if the connection is gone.
            let connection_exists =
                predefined_graph.graph.connections().as_ref().is_some_and(|connections| {
                    connections.iter().any(|conn| {
                        conn.loc.extension == Some("extension_1".to_string())
                            && conn
                                .loc
                                .app
                                .as_ref()
                                .is_some_and(|app| app == "http://example.com:8000")
                            && conn.cmd.as_ref().is_some_and(|cmds| {
                                cmds.iter().any(|cmd| {
                                    cmd.names.as_ref().is_some_and(|names| {
                                        names.iter().any(|name| name == "multi_name_1")
                                            || names.iter().any(|name| name == "multi_name_2")
                                            || names.iter().any(|name| name == "multi_name_3")
                                    })
                                })
                            })
                    })
                });
            assert!(!connection_exists, "Connection should have been deleted");
        } else {
            panic!("Graph 'default_with_app_uri' not found");
        }

        // Read the updated property.json file.
        let updated_property_content = std::fs::read_to_string(&property_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_content).unwrap();

        // Create the expected property JSON, which is the same as input but
        // with the connection removed.
        let expected_property_json_str = include_str!("../../../../test_data/app_property.json");

        // Parse the expected property JSON.
        let expected_property: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();

        assert_eq!(
            updated_property,
            expected_property,
            "Property file doesn't match expected content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_property).unwrap(),
            serde_json::to_string_pretty(&updated_property).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_delete_graph_connection_multiple_msg_names_remove_connection_success() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str =
            include_str!("../../../../test_data/app_property_multi_names_flow_2.json");
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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/delete",
                web::post().to(delete_graph_connection_endpoint),
            ))
            .await;

        // Delete a connection from the default_with_app_uri graph.
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
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec![
                "multi_name_1".to_string(),
                "multi_name_2".to_string(),
                "multi_name_3".to_string(),
            ],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should succeed with a 200 OK
        assert_eq!(resp.status(), 200);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ApiResponse<DeleteGraphConnectionResponsePayload> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Ok);
        assert!(response.data.success);

        // Verify the connection was actually removed from the data.
        let graphs_cache = designer_state.graphs_cache.read().await;
        if let Some(predefined_graph) = graphs_cache_find_by_id(&graphs_cache, &graph_id_clone) {
            // Check if the connection is gone.
            let connection_exists =
                predefined_graph.graph.connections().as_ref().is_some_and(|connections| {
                    connections.iter().any(|conn| {
                        conn.loc.extension == Some("extension_1".to_string())
                            && conn
                                .loc
                                .app
                                .as_ref()
                                .is_some_and(|app| app == "http://example.com:8000")
                            && conn.cmd.as_ref().is_some_and(|cmds| {
                                cmds.iter().any(|cmd| {
                                    cmd.names.as_ref().is_some_and(|names| {
                                        names.iter().any(|name| name == "multi_name_1")
                                            || names.iter().any(|name| name == "multi_name_2")
                                            || names.iter().any(|name| name == "multi_name_3")
                                    })
                                })
                            })
                    })
                });
            assert!(!connection_exists, "Connection should have been deleted");
        } else {
            panic!("Graph 'default_with_app_uri' not found");
        }

        // Read the updated property.json file.
        let updated_property_content = std::fs::read_to_string(&property_path).unwrap();

        // Parse the contents as JSON for proper comparison.
        let updated_property: serde_json::Value =
            serde_json::from_str(&updated_property_content).unwrap();

        // Create the expected property JSON, which is the same as input but
        // with the connection removed.
        let expected_property_json_str =
            include_str!("../../../../test_data/expected_property_delete_connection.json");

        // Parse the expected property JSON.
        let expected_property: serde_json::Value =
            serde_json::from_str(expected_property_json_str).unwrap();

        assert_eq!(
            updated_property,
            expected_property,
            "Property file doesn't match expected content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&expected_property).unwrap(),
            serde_json::to_string_pretty(&updated_property).unwrap()
        );
    }

    #[actix_web::test]
    async fn test_delete_graph_connection_multiple_msg_names_across_flows_invalid() {
        // Create a test directory with property.json file.
        let temp_dir = tempfile::tempdir().unwrap();
        let temp_dir_path = temp_dir.path().to_str().unwrap().to_string();

        // Read test data from embedded JSON files.
        let input_property_json_str =
            include_str!("../../../../test_data/app_property_multi_names_flow.json");
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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/connections/delete",
                web::post().to(delete_graph_connection_endpoint),
            ))
            .await;

        // Delete a connection from the default_with_app_uri graph.
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
        let request_payload = DeleteGraphConnectionRequestPayload {
            graph_id: graph_id_clone,
            src,
            dest,
            msg_type: MsgType::Cmd,
            msg_names: vec![
                "multi_name_1".to_string(),
                "multi_name_2".to_string(),
                "hello_world".to_string(),
            ],
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/connections/delete")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Should fail with a 400 Bad Request for nonexistent connections.
        assert_eq!(resp.status(), 400);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        let response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(response.status, Status::Fail);
    }
}
