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
                connections::{
                    DesignerGraphConnection, DesignerGraphDestination, DesignerGraphLoc,
                    DesignerGraphMessageFlow,
                },
                get::{get_graphs_endpoint, GetGraphsRequestPayload},
                DesignerGraphInfo,
            },
            response::ApiResponse,
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
    async fn test_get_connections_success() {
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

        // Find the UUID for the graph with name "default"
        let default_graph_uuid;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            default_graph_uuid = graphs_cache
                .iter()
                .find_map(|(uuid, graph)| {
                    if graph.name.as_ref().is_some_and(|name| name == "default") {
                        Some(*uuid)
                    } else {
                        None
                    }
                })
                .expect("No graph with name 'default' found");
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
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

        let graphs_response: ApiResponse<Vec<DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();

        // Find the graph with the expected UUID and extract its connections
        let connections = graphs_response
            .data
            .iter()
            .find(|graph| graph.graph_id == default_graph_uuid)
            .expect("Graph not found")
            .graph
            .connections
            .clone();

        let expected_connections = vec![DesignerGraphConnection {
            loc: DesignerGraphLoc {
                app: None,
                extension: Some("extension_1".to_string()),
                subgraph: None,
                selector: None,
            },
            cmd: Some(vec![DesignerGraphMessageFlow {
                name: Some("hello_world".to_string()),
                names: None,
                dest: vec![DesignerGraphDestination {
                    loc: DesignerGraphLoc {
                        app: None,
                        extension: Some("extension_2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                source: vec![],
            }]),
            data: None,
            audio_frame: None,
            video_frame: None,
        }];

        assert_eq!(connections, expected_connections);
        assert!(!connections.is_empty());

        let pretty_json = serde_json::to_string_pretty(&graphs_response).unwrap();
        println!("Response body: {pretty_json}");
    }

    #[actix_web::test]
    async fn test_get_connections_have_all_data_type() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // The first item is 'manifest.json', and the second item is
        // 'property.json'.
        let all_pkgs_json_str = vec![
            (
                TEST_DIR.to_string(),
                include_str!(
                    "../../../../test_data/get_connections_have_all_data_type/app_manifest.json"
                )
                .to_string(),
                include_str!(
                    "../../../../test_data/get_connections_have_all_data_type/app_property.json"
                )
                .to_string(),
            ),
            (
                format!("{}{}", TEST_DIR, "/ten_packages/extension/extension_addon_1"),
                include_str!(
                    "../../../../test_data/get_connections_have_all_data_type/\
                     extension_addon_1_manifest.json"
                )
                .to_string(),
                "{}".to_string(),
            ),
            (
                format!("{}{}", TEST_DIR, "/ten_packages/extension/extension_addon_2"),
                include_str!(
                    "../../../../test_data/get_connections_have_all_data_type/\
                     extension_addon_2_manifest.json"
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

        // Find the UUID for the graph with name "default"
        let default_graph_uuid;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            default_graph_uuid = graphs_cache
                .iter()
                .find_map(|(uuid, graph)| {
                    if graph.name.as_ref().is_some_and(|name| name == "default") {
                        Some(*uuid)
                    } else {
                        None
                    }
                })
                .expect("No graph with name 'default' found");
        }

        let designer_state = Arc::new(designer_state);
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
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

        let graphs_response: ApiResponse<Vec<DesignerGraphInfo>> =
            serde_json::from_str(body_str).unwrap();

        // Find the graph with the expected UUID and extract its connections
        let connections = graphs_response
            .data
            .iter()
            .find(|graph| graph.graph_id == default_graph_uuid)
            .expect("Graph not found")
            .graph
            .connections
            .clone();

        let expected_connections = vec![DesignerGraphConnection {
            loc: DesignerGraphLoc {
                app: None,
                extension: Some("extension_1".to_string()),
                subgraph: None,
                selector: None,
            },
            cmd: Some(vec![DesignerGraphMessageFlow {
                name: Some("hello_world".to_string()),
                names: None,
                dest: vec![DesignerGraphDestination {
                    loc: DesignerGraphLoc {
                        app: None,
                        extension: Some("extension_2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                source: vec![],
            }]),
            data: Some(vec![DesignerGraphMessageFlow {
                name: Some("data".to_string()),
                names: None,
                dest: vec![DesignerGraphDestination {
                    loc: DesignerGraphLoc {
                        app: None,
                        extension: Some("extension_2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                source: vec![],
            }]),
            audio_frame: Some(vec![DesignerGraphMessageFlow {
                name: Some("pcm".to_string()),
                names: None,
                dest: vec![DesignerGraphDestination {
                    loc: DesignerGraphLoc {
                        app: None,
                        extension: Some("extension_2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                source: vec![],
            }]),
            video_frame: Some(vec![DesignerGraphMessageFlow {
                name: Some("image".to_string()),
                names: None,
                dest: vec![DesignerGraphDestination {
                    loc: DesignerGraphLoc {
                        app: None,
                        extension: Some("extension_2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                source: vec![],
            }]),
        }];

        assert_eq!(connections, expected_connections);
        assert!(!connections.is_empty());

        let pretty_json = serde_json::to_string_pretty(&graphs_response).unwrap();
        println!("Response body: {pretty_json}");
    }
}
