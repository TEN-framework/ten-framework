//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{http::StatusCode, test, web};
use serde_json::json;
use ten_manager::{
    designer::{
        response::ApiResponse,
        storage::{
            in_memory::TmanStorageInMemory,
            persistent::{
                get::{get_persistent_storage_endpoint, GetPersistentResponseData},
                schema::{set_persistent_storage_schema_endpoint, SetSchemaRequestPayload},
                set::{
                    set_persistent_storage_endpoint, SetPersistentRequestPayload,
                    SetPersistentResponseData,
                },
            },
        },
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

use crate::test_case::common::temp_home::TempHome;

#[actix_web::test]
async fn test_set_and_get_persistent_simple() {
    let _temp_home = TempHome::new();

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

    // Create app with all endpoints.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route(
                "/storage/persistent/schema",
                web::post().to(set_persistent_storage_schema_endpoint),
            )
            .route("/storage/persistent/set", web::post().to(set_persistent_storage_endpoint))
            .route("/storage/persistent/get", web::post().to(get_persistent_storage_endpoint)),
    )
    .await;

    // Set up schema first - use a very permissive schema for testing
    let schema = json!({
        "type": "object",
        "additionalProperties": true
    });

    let schema_payload = SetSchemaRequestPayload {
        schema,
    };

    let schema_req = test::TestRequest::post()
        .uri("/storage/persistent/schema")
        .set_json(&schema_payload)
        .to_request();

    let schema_resp = test::call_service(&app, schema_req).await;
    assert_eq!(schema_resp.status(), StatusCode::OK);

    // Test setting a simple value
    let set_payload = SetPersistentRequestPayload {
        key: "custom_data.test_value".to_string(),
        value: json!({"nodes_geometry": [{"x": 100, "y": 200}]}),
    };

    let set_req = test::TestRequest::post()
        .uri("/storage/persistent/set")
        .set_json(&set_payload)
        .to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    let set_body = test::read_body(set_resp).await;
    let set_result: ApiResponse<SetPersistentResponseData> =
        serde_json::from_slice(&set_body).unwrap();
    assert!(set_result.data.success);

    // Test getting the same value
    let get_payload = json!({"key": "custom_data.test_value"});

    let get_req = test::TestRequest::post()
        .uri("/storage/persistent/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetPersistentResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_some());
    let returned_value = get_result.data.value.unwrap();
    assert_eq!(returned_value, json!({"nodes_geometry": [{"x": 100, "y": 200}]}));
}

#[actix_web::test]
async fn test_set_and_get_persistent_array_key() {
    let _temp_home = TempHome::new();

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
            .route(
                "/storage/persistent/schema",
                web::post().to(set_persistent_storage_schema_endpoint),
            )
            .route("/storage/persistent/set", web::post().to(set_persistent_storage_endpoint))
            .route("/storage/persistent/get", web::post().to(get_persistent_storage_endpoint)),
    )
    .await;

    // Set up schema first - use a very permissive schema for testing
    let schema = json!({
        "type": "object",
        "additionalProperties": true
    });

    let schema_payload = SetSchemaRequestPayload {
        schema,
    };

    let schema_req = test::TestRequest::post()
        .uri("/storage/persistent/schema")
        .set_json(&schema_payload)
        .to_request();

    let schema_resp = test::call_service(&app, schema_req).await;
    assert_eq!(schema_resp.status(), StatusCode::OK);

    // Test setting a value with array key
    let set_payload = SetPersistentRequestPayload {
        key: "nodes[1].position".to_string(),
        value: json!({"x": 300, "y": 400}),
    };

    let set_req = test::TestRequest::post()
        .uri("/storage/persistent/set")
        .set_json(&set_payload)
        .to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    // Test getting the value
    let get_payload = json!({"key": "nodes[1].position"});

    let get_req = test::TestRequest::post()
        .uri("/storage/persistent/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetPersistentResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_some());
    let returned_value = get_result.data.value.unwrap();
    assert_eq!(returned_value, json!({"x": 300, "y": 400}));
}

#[actix_web::test]
async fn test_get_persistent_nonexistent_key() {
    let _temp_home = TempHome::new();

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
            .route(
                "/storage/persistent/schema",
                web::post().to(set_persistent_storage_schema_endpoint),
            )
            .route("/storage/persistent/get", web::post().to(get_persistent_storage_endpoint)),
    )
    .await;

    // Set up schema first - use a very permissive schema for testing
    let schema = json!({
        "type": "object",
        "additionalProperties": true
    });

    let schema_payload = SetSchemaRequestPayload {
        schema,
    };

    let schema_req = test::TestRequest::post()
        .uri("/storage/persistent/schema")
        .set_json(&schema_payload)
        .to_request();

    let schema_resp = test::call_service(&app, schema_req).await;
    assert_eq!(schema_resp.status(), StatusCode::OK);

    // Test getting a nonexistent key
    let get_payload = json!({"key": "nonexistent.key"});

    let get_req = test::TestRequest::post()
        .uri("/storage/persistent/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetPersistentResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_none());
}

#[actix_web::test]
async fn test_get_persistent_without_schema_fails() {
    let _temp_home = TempHome::new();

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
            .route("/storage/persistent/get", web::post().to(get_persistent_storage_endpoint)),
    )
    .await;

    // Test getting without setting schema first - should fail
    let get_payload = json!({"key": "nonexistent.key"});

    let get_req = test::TestRequest::post()
        .uri("/storage/persistent/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::BAD_REQUEST);
}

#[actix_web::test]
async fn test_persistent_storage_persists_across_requests() {
    let _temp_home = TempHome::new();

    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create first app instance
    let app1 = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route(
                "/storage/persistent/schema",
                web::post().to(set_persistent_storage_schema_endpoint),
            )
            .route("/storage/persistent/set", web::post().to(set_persistent_storage_endpoint)),
    )
    .await;

    // Set up schema first - use a very permissive schema for testing
    let schema = json!({
        "type": "object",
        "additionalProperties": true
    });

    let schema_payload = SetSchemaRequestPayload {
        schema: schema.clone(),
    };

    let schema_req = test::TestRequest::post()
        .uri("/storage/persistent/schema")
        .set_json(&schema_payload)
        .to_request();

    let schema_resp = test::call_service(&app1, schema_req).await;
    assert_eq!(schema_resp.status(), StatusCode::OK);

    // Set a value using first app instance
    let set_payload = SetPersistentRequestPayload {
        key: "persistent_test".to_string(),
        value: json!("persistent_value"),
    };

    let set_req = test::TestRequest::post()
        .uri("/storage/persistent/set")
        .set_json(&set_payload)
        .to_request();

    let set_resp = test::call_service(&app1, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    // Create second app instance (simulating restart)
    let app2 = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route(
                "/storage/persistent/schema",
                web::post().to(set_persistent_storage_schema_endpoint),
            )
            .route("/storage/persistent/get", web::post().to(get_persistent_storage_endpoint)),
    )
    .await;

    // Set up schema for second app instance (simulating schema being set again
    // after restart)
    let schema_payload2 = SetSchemaRequestPayload {
        schema,
    };

    let schema_req2 = test::TestRequest::post()
        .uri("/storage/persistent/schema")
        .set_json(&schema_payload2)
        .to_request();

    let schema_resp2 = test::call_service(&app2, schema_req2).await;
    assert_eq!(schema_resp2.status(), StatusCode::OK);

    // Get the value using second app instance
    let get_payload = json!({"key": "persistent_test"});

    let get_req = test::TestRequest::post()
        .uri("/storage/persistent/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app2, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetPersistentResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    assert!(get_result.data.value.is_some());
    assert_eq!(get_result.data.value.unwrap(), json!("persistent_value"));
}

#[actix_web::test]
async fn test_set_persistent_invalid_key() {
    let _temp_home = TempHome::new();

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
            .route(
                "/storage/persistent/schema",
                web::post().to(set_persistent_storage_schema_endpoint),
            )
            .route("/storage/persistent/set", web::post().to(set_persistent_storage_endpoint)),
    )
    .await;

    // Set up schema first - use a very permissive schema for testing
    let schema = json!({
        "type": "object",
        "additionalProperties": true
    });

    let schema_payload = SetSchemaRequestPayload {
        schema,
    };

    let schema_req = test::TestRequest::post()
        .uri("/storage/persistent/schema")
        .set_json(&schema_payload)
        .to_request();

    let schema_resp = test::call_service(&app, schema_req).await;
    assert_eq!(schema_resp.status(), StatusCode::OK);

    // Test setting with invalid key (contains uppercase)
    let set_payload = SetPersistentRequestPayload {
        key: "Graph_ui.test".to_string(),
        value: json!("test_value"),
    };

    let set_req = test::TestRequest::post()
        .uri("/storage/persistent/set")
        .set_json(&set_payload)
        .to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::BAD_REQUEST);
}

#[actix_web::test]
async fn test_set_persistent_without_schema_fails() {
    let _temp_home = TempHome::new();

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
            .route("/storage/persistent/set", web::post().to(set_persistent_storage_endpoint)),
    )
    .await;

    // Test setting without schema first - should fail
    let set_payload = SetPersistentRequestPayload {
        key: "test.key".to_string(),
        value: json!("test_value"),
    };

    let set_req = test::TestRequest::post()
        .uri("/storage/persistent/set")
        .set_json(&set_payload)
        .to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::BAD_REQUEST);
}
