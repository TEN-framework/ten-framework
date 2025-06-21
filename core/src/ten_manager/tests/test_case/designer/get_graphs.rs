//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{http::StatusCode, test, web, App};

use ten_manager::{
    designer::{
        graphs::{
            connections::get::{
                get_graph_connections_endpoint,
                GetGraphConnectionsRequestPayload,
                GraphConnectionsSingleResponseData,
            },
            get::{
                get_graphs_endpoint, GetGraphsRequestPayload,
                GetGraphsResponseData,
            },
        },
        response::ApiResponse,
        storage::in_memory::TmanStorageInMemory,
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

use crate::test_case::common::mock::inject_all_pkgs_for_mock;

#[actix_rt::test]
async fn test_cmd_designer_graphs_app_property_not_exist() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(
            TmanStorageInMemory::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };

    let all_pkgs_json_str = vec![
        (
            "tests/test_data/cmd_designer_graphs_app_property_not_exist"
                .to_string(),
            include_str!(
                "../../test_data/cmd_designer_graphs_app_property_not_exist/\
                 manifest.json"
            )
            .to_string(),
            "{}".to_string(),
        ),
        (
            "tests/test_data/cmd_designer_graphs_app_property_not_exist/\
             ten_packages/extension/addon_a"
                .to_string(),
            include_str!(
                "../../test_data/cmd_designer_graphs_app_property_not_exist/\
                 ten_packages/extension/addon_a/manifest.json"
            )
            .to_string(),
            "{}".to_string(),
        ),
        (
            "tests/test_data/cmd_designer_graphs_app_property_not_exist/\
             ten_packages/extension/addon_b"
                .to_string(),
            include_str!(
                "../../test_data/cmd_designer_graphs_app_property_not_exist/\
                 ten_packages/extension/addon_b/manifest.json"
            )
            .to_string(),
            "{}".to_string(),
        ),
    ];

    {
        let mut pkgs_cache = designer_state.pkgs_cache.write().await;
        let mut graphs_cache = designer_state.graphs_cache.write().await;

        let inject_ret = inject_all_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            all_pkgs_json_str,
        )
        .await;
        assert!(inject_ret.is_ok());
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
        .set_json(&request_payload)
        .to_request();
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<Vec<GetGraphsResponseData>> =
        serde_json::from_str(body_str).unwrap();

    let pretty_json = serde_json::to_string_pretty(&json).unwrap();
    println!("Response body: {pretty_json}");

    assert!(json.data.is_empty());
}

#[actix_rt::test]
async fn test_cmd_designer_connections_has_msg_conversion() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(
            TmanStorageInMemory::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };

    let all_pkgs_json_str = vec![
        (
            "tests/test_data/cmd_designer_connections_has_msg_conversion"
                .to_string(),
            include_str!(
                "../../test_data/cmd_designer_connections_has_msg_conversion/\
                 manifest.json"
            )
            .to_string(),
            include_str!(
                "../../test_data/cmd_designer_connections_has_msg_conversion/\
                 property.json"
            )
            .to_string(),
        ),
        (
            "tests/test_data/cmd_designer_connections_has_msg_conversion/\
             ten_packages/extension/addon_a"
                .to_string(),
            include_str!(
                "../../test_data/cmd_designer_connections_has_msg_conversion/\
                 ten_packages/extension/addon_a/manifest.json"
            )
            .to_string(),
            "{}".to_string(),
        ),
        (
            "tests/test_data/cmd_designer_connections_has_msg_conversion/\
             ten_packages/extension/addon_b"
                .to_string(),
            include_str!(
                "../../test_data/cmd_designer_connections_has_msg_conversion/\
                 ten_packages/extension/addon_b/manifest.json"
            )
            .to_string(),
            "{}".to_string(),
        ),
    ];

    {
        let mut pkgs_cache = designer_state.pkgs_cache.write().await;
        let mut graphs_cache = designer_state.graphs_cache.write().await;

        let inject_ret = inject_all_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            all_pkgs_json_str,
        )
        .await;
        assert!(inject_ret.is_ok());
    }

    let designer_state = Arc::new(designer_state);
    let app = test::init_service(
        App::new().app_data(web::Data::new(designer_state.clone())).route(
            "/api/designer/v1/graphs/connections",
            web::post().to(get_graph_connections_endpoint),
        ),
    )
    .await;

    let request_payload = GetGraphConnectionsRequestPayload {
        graph_id: {
            let graphs_cache = designer_state.graphs_cache.read().await;

            graphs_cache
                .iter()
                .find_map(|(uuid, graph)| {
                    if graph.name.as_ref().is_some_and(|name| name == "default")
                    {
                        Some(*uuid)
                    } else {
                        None
                    }
                })
                .expect("No graph with name 'default' found")
        },
    };

    let req = test::TestRequest::post()
        .uri("/api/designer/v1/graphs/connections")
        .set_json(&request_payload)
        .to_request();
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<Vec<GraphConnectionsSingleResponseData>> =
        serde_json::from_str(body_str).unwrap();

    let pretty_json = serde_json::to_string_pretty(&json).unwrap();
    println!("Response body: {pretty_json}");

    let connections = &json.data;
    assert_eq!(connections.len(), 1);

    let connection = connections.first().unwrap();
    assert!(connection.cmd.is_some());

    let cmd = connection.cmd.as_ref().unwrap();
    assert_eq!(cmd.len(), 1);
}
