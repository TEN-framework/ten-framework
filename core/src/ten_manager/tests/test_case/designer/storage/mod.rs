//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod in_memory;
mod persistent;
mod persistent_schema_test;

use std::{collections::HashMap, sync::Arc};

use actix_web::{http::StatusCode, test, web};
use serde_json::json;
use ten_manager::{
    designer::{
        response::ApiResponse,
        storage::in_memory::{
            get::{get_in_memory_storage_endpoint, GetMemoryResponseData},
            set::{set_in_memory_storage_endpoint, SetMemoryRequestPayload, SetMemoryResponseData},
            TmanStorageInMemory,
        },
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

#[actix_web::test]
async fn test_set_and_get_memory_simple() {
    // Create a clean state with empty config.
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with both endpoints.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/storage/memory/set", web::post().to(set_in_memory_storage_endpoint))
            .route("/storage/memory/get", web::post().to(get_in_memory_storage_endpoint)),
    )
    .await;

    // Test setting a simple value using a simple key that doesn't conflict with
    // TmanStorageInMemory structure
    let set_payload = SetMemoryRequestPayload {
        key: "custom_data.test_value".to_string(),
        value: json!({"nodes_geometry": [{"x": 100, "y": 200}]}),
    };

    let set_req =
        test::TestRequest::post().uri("/storage/memory/set").set_json(&set_payload).to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    let set_body = test::read_body(set_resp).await;
    let set_result: ApiResponse<SetMemoryResponseData> = serde_json::from_slice(&set_body).unwrap();
    assert!(set_result.data.success);

    // Test getting the same value
    let get_payload = json!({"key": "custom_data.test_value"});

    let get_req =
        test::TestRequest::post().uri("/storage/memory/get").set_json(&get_payload).to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetMemoryResponseData> = serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_some());
    let returned_value = get_result.data.value.unwrap();
    assert_eq!(returned_value, json!({"nodes_geometry": [{"x": 100, "y": 200}]}));
}

#[actix_web::test]
async fn test_set_and_get_memory_array_key() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/storage/memory/set", web::post().to(set_in_memory_storage_endpoint))
            .route("/storage/memory/get", web::post().to(get_in_memory_storage_endpoint)),
    )
    .await;

    // Test setting a value with array key
    let set_payload = SetMemoryRequestPayload {
        key: "nodes[1].position".to_string(),
        value: json!({"x": 300, "y": 400}),
    };

    let set_req =
        test::TestRequest::post().uri("/storage/memory/set").set_json(&set_payload).to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    // Test getting the value
    let get_payload = json!({"key": "nodes[1].position"});

    let get_req =
        test::TestRequest::post().uri("/storage/memory/get").set_json(&get_payload).to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetMemoryResponseData> = serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_some());
    let returned_value = get_result.data.value.unwrap();
    assert_eq!(returned_value, json!({"x": 300, "y": 400}));
}

#[actix_web::test]
async fn test_get_memory_nonexistent_key() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/storage/memory/get", web::post().to(get_in_memory_storage_endpoint)),
    )
    .await;

    // Test getting a nonexistent key
    let get_payload = json!({"key": "nonexistent.key"});

    let get_req =
        test::TestRequest::post().uri("/storage/memory/get").set_json(&get_payload).to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetMemoryResponseData> = serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_none());
}

#[actix_web::test]
async fn test_set_memory_invalid_key() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/storage/memory/set", web::post().to(set_in_memory_storage_endpoint)),
    )
    .await;

    // Test setting with invalid key (contains uppercase)
    let set_payload = SetMemoryRequestPayload {
        key: "Graph_ui.test".to_string(),
        value: json!("test_value"),
    };

    let set_req =
        test::TestRequest::post().uri("/storage/memory/set").set_json(&set_payload).to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::BAD_REQUEST);
}

#[actix_web::test]
async fn test_overwrite_existing_key() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/storage/memory/set", web::post().to(set_in_memory_storage_endpoint))
            .route("/storage/memory/get", web::post().to(get_in_memory_storage_endpoint)),
    )
    .await;

    let key = "test_key".to_string();

    // Set initial value
    let set_payload = SetMemoryRequestPayload {
        key: key.clone(),
        value: json!("initial_value"),
    };

    let set_req =
        test::TestRequest::post().uri("/storage/memory/set").set_json(&set_payload).to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    // Set new value (overwrite)
    let set_payload = SetMemoryRequestPayload {
        key: key.clone(),
        value: json!("updated_value"),
    };

    let set_req =
        test::TestRequest::post().uri("/storage/memory/set").set_json(&set_payload).to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    // Get the value and verify it's been updated
    let get_payload = json!({"key": key});

    let get_req =
        test::TestRequest::post().uri("/storage/memory/get").set_json(&get_payload).to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetMemoryResponseData> = serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_some());
    assert_eq!(get_result.data.value.unwrap(), json!("updated_value"));
}
