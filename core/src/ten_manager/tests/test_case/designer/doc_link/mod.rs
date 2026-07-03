//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{http::StatusCode, test, web};
use ten_manager::{
    designer::{
        doc_link::{
            get_doc_link_endpoint, DocLinkKey, GetDocLinkRequestPayload, GetDocLinkResponseData,
        },
        locale::Locale,
        response::ApiResponse,
        storage::in_memory::TmanStorageInMemory,
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

#[actix_web::test]
async fn test_get_doc_link_success() {
    // Create test data with default locale.
    let payload = GetDocLinkRequestPayload {
        key: DocLinkKey::Graph,
        locale: Locale::EnUs,
    };

    // Get mock state.
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with the endpoint.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/doc_link", web::post().to(get_doc_link_endpoint)),
    )
    .await;

    // Make the request.
    let req = test::TestRequest::post().uri("/doc_link").set_json(&payload).to_request();

    let resp = test::call_service(&app, req).await;

    // Assert status code.
    assert_eq!(resp.status(), StatusCode::OK);

    // Extract and check the response body.
    let body = test::read_body(resp).await;
    let result: ApiResponse<GetDocLinkResponseData> = serde_json::from_slice(&body).unwrap();

    // Assert data.
    assert_eq!(result.data.key, DocLinkKey::Graph);
    assert_eq!(result.data.locale, Locale::EnUs);
    assert!(!result.data.text.is_empty());
}

#[actix_web::test]
async fn test_get_doc_link_chinese() {
    // Create test data with Chinese locale.
    let payload = GetDocLinkRequestPayload {
        key: DocLinkKey::Graph,
        locale: Locale::ZhCn,
    };

    // Get mock state.
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with the endpoint.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/doc_link", web::post().to(get_doc_link_endpoint)),
    )
    .await;

    // Make the request.
    let req = test::TestRequest::post().uri("/doc_link").set_json(&payload).to_request();

    let resp = test::call_service(&app, req).await;

    // Assert status code.
    assert_eq!(resp.status(), StatusCode::OK);

    // Extract and check the response body.
    let body = test::read_body(resp).await;
    let result: ApiResponse<GetDocLinkResponseData> = serde_json::from_slice(&body).unwrap();

    // Assert data.
    assert_eq!(result.data.key, DocLinkKey::Graph);
    assert_eq!(result.data.locale, Locale::ZhCn);
    // Chinese text should be different from English.
    assert!(!result.data.text.is_empty());
}

#[actix_web::test]
async fn test_get_doc_link_language_fallback() {
    // Use a non-existent locale.
    let payload = GetDocLinkRequestPayload {
        key: DocLinkKey::Graph,
        // Use a locale that definitely doesn't have content.
        locale: Locale::JaJp,
    };

    // Get mock state.
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with the endpoint.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/doc_link", web::post().to(get_doc_link_endpoint)),
    )
    .await;

    // Make the request.
    let req = test::TestRequest::post().uri("/doc_link").set_json(&payload).to_request();

    let resp = test::call_service(&app, req).await;

    // Assert status code.
    assert_eq!(resp.status(), StatusCode::OK);

    // Extract and check the response body.
    let body = test::read_body(resp).await;
    let result: ApiResponse<GetDocLinkResponseData> = serde_json::from_slice(&body).unwrap();

    // Assert data - should fall back to another locale.
    assert_eq!(result.data.key, DocLinkKey::Graph);
    // The locale in the response should be the one that was actually used.
    assert!(result.data.locale != Locale::JaJp);
}

#[actix_web::test]
async fn test_get_doc_link_not_found() {
    // Use an unknown key.
    let payload = GetDocLinkRequestPayload {
        key: DocLinkKey::Unknown,
        locale: Locale::EnUs,
    };

    // Get mock state.
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with the endpoint.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/doc_link", web::post().to(get_doc_link_endpoint)),
    )
    .await;

    // Make the request.
    let req = test::TestRequest::post().uri("/doc_link").set_json(&payload).to_request();

    let resp = test::call_service(&app, req).await;

    // Assert status code.
    assert_eq!(resp.status(), StatusCode::NOT_FOUND);
}

#[actix_web::test]
async fn test_default_locale() {
    // Test the default_locale function indirectly by deserializing JSON without
    // a locale field.
    let json = r#"{"key": "graph"}"#;
    let request: GetDocLinkRequestPayload = serde_json::from_str(json).unwrap();

    assert_eq!(request.locale, Locale::EnUs);
}
