//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod install_all;

use std::{collections::HashMap, sync::Arc};

use actix_web::{
    http::{header, StatusCode},
    test, web, App,
};
use ten_manager::{
    designer::{
        builtin_function::builtin_function_endpoint, storage::in_memory::TmanStorageInMemory,
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

#[actix_rt::test]
async fn test_cmd_builtin_function_websocket_connection() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };

    let designer_state = Arc::new(designer_state);

    // Initialize the test service with the WebSocket endpoint.
    let app = test::init_service(
        App::new()
            .app_data(web::Data::new(designer_state))
            .route("/ws/builtin-function", web::get().to(builtin_function_endpoint)),
    )
    .await;

    // Create a test request with proper WebSocket headers.
    let req = test::TestRequest::get()
        .uri("/ws/builtin-function")
        .insert_header((header::CONNECTION, "upgrade"))
        .insert_header((header::UPGRADE, "websocket"))
        .insert_header(("Sec-WebSocket-Version", "13"))
        // Base64 encoded value.
        .insert_header(("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ=="))
        .to_request();

    // Call the WebSocket endpoint.
    let resp = test::call_service(&app, req).await;

    // When a WebSocket connection is established successfully, the server
    // should respond with:
    // 1. A 101 Switching Protocols status code
    // 2. Various WebSocket headers

    println!("WebSocket connection response: {:?}", resp.status());
    println!("WebSocket connection headers: {:#?}", resp.headers());

    // Verify that we got the correct WebSocket upgrade response.
    assert_eq!(
        resp.status(),
        StatusCode::SWITCHING_PROTOCOLS,
        "Expected WebSocket upgrade response (101 Switching Protocols)"
    );

    // Check for key WebSocket response headers.
    assert!(resp.headers().contains_key("Sec-WebSocket-Accept"), "Missing WebSocket Accept header");
    assert_eq!(
        resp.headers().get(header::UPGRADE).unwrap().to_str().unwrap().to_lowercase(),
        "websocket",
        "Incorrect upgrade header value"
    );

    println!(
        "WebSocket connection was successfully established (validated by checking response status \
         and headers)"
    );
}
