//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{test, web, App};
use serde_json;
use ten_manager::{
    designer::{
        preferences::logviewer_line_size::{
            get_logviewer_line_size_endpoint, update_logviewer_line_size_endpoint,
            UpdateLogviewerLineSizeRequestPayload,
        },
        storage::in_memory::TmanStorageInMemory,
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

#[actix_web::test]
async fn test_get_logviewer_line_size_success() {
    // Create test state.
    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Create test app.
    let app = test::init_service(App::new().app_data(web::Data::new(state)).service(
        web::scope("/api/designer/v1").route(
            "/preferences/logviewer_line_size",
            web::get().to(get_logviewer_line_size_endpoint),
        ),
    ))
    .await;

    // Create test request.
    let req = test::TestRequest::get()
        .uri("/api/designer/v1/preferences/logviewer_line_size")
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 200 OK.
    assert!(resp.status().is_success());

    // Parse response body.
    let body = test::read_body(resp).await;
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();

    // Assert response structure.
    assert_eq!(json["status"], "ok");
    assert!(json["data"].is_object());
    assert!(json["data"]["logviewer_line_size"].is_number());
}

#[actix_web::test]
async fn test_update_logviewer_line_size_success() {
    // Create test state with mock config file path to avoid writing to real
    // file.
    let config = TmanConfig {
        config_file: None,
        ..TmanConfig::default()
    };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Create test app.
    let app = test::init_service(App::new().app_data(web::Data::new(state.clone())).service(
        web::scope("/api/designer/v1").route(
            "/preferences/logviewer_line_size",
            web::put().to(update_logviewer_line_size_endpoint),
        ),
    ))
    .await;

    // Create valid payload.
    let payload = UpdateLogviewerLineSizeRequestPayload {
        logviewer_line_size: 3000,
    };

    // Create test request.
    let req = test::TestRequest::put()
        .uri("/api/designer/v1/preferences/logviewer_line_size")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 200 OK.
    assert!(resp.status().is_success());

    // Parse response body.
    let body = test::read_body(resp).await;
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();

    // Assert response structure.
    assert_eq!(json["status"], "ok");

    // Verify config was updated.
    assert_eq!(state.tman_config.read().await.designer.logviewer_line_size, 3000);
}

#[actix_web::test]
async fn test_update_logviewer_line_size_invalid_value() {
    // Create test state.
    let config = TmanConfig {
        config_file: None,
        ..TmanConfig::default()
    };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Create test app.
    let app = test::init_service(App::new().app_data(web::Data::new(state)).service(
        web::scope("/api/designer/v1").route(
            "/preferences/logviewer_line_size",
            web::put().to(update_logviewer_line_size_endpoint),
        ),
    ))
    .await;

    // Create invalid payload.
    let payload = UpdateLogviewerLineSizeRequestPayload {
        logviewer_line_size: 0,
    };

    // Create test request.
    let req = test::TestRequest::put()
        .uri("/api/designer/v1/preferences/logviewer_line_size")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 400 Bad Request.
    assert_eq!(resp.status(), 400);
}
