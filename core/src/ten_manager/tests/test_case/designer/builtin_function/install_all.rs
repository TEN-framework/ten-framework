//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{test, web, App};
use futures_util::{SinkExt, StreamExt};
use ten_manager::{
    designer::{
        builtin_function::{builtin_function_endpoint, msg::InboundMsg},
        storage::in_memory::TmanStorageInMemory,
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};

use crate::test_case::common::builtin_server::start_test_server;

#[actix_rt::test]
async fn test_ws_builtin_function_install_all() {
    // Start the WebSocket server and get its address
    let server_addr =
        start_test_server("/ws/builtin-function", || web::get().to(builtin_function_endpoint))
            .await;
    println!("Server started at: {server_addr}");

    // Connect WebSocket client
    let ws_url = format!("ws://{server_addr}/ws/builtin-function");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("WebSocket connection established");

    // Split the WebSocket stream.
    let (mut write, mut read) = ws_stream.split();

    // Prepare InstallAll message.
    let install_all_msg = InboundMsg::InstallAll {
        base_dir: "tests/test_data/cmd_builtin_function_install_all".to_string(),
    };
    let json_msg = serde_json::to_string(&install_all_msg).unwrap();

    // Send the message.
    write.send(Message::Text(json_msg.into())).await.unwrap();
    println!("Sent InstallAll message: {}", serde_json::to_string(&install_all_msg).unwrap());

    // Wait for all response messages until server disconnects.
    let mut message_count = 0;
    let mut last_text_message = String::new();

    while let Some(Ok(message)) = read.next().await {
        match message {
            Message::Text(text) => {
                println!("Received response #{}: {}", message_count + 1, text);
                // Verify that the response is not empty.
                assert!(!text.is_empty(), "Response should not be empty");
                // Save the text message for later verification.
                last_text_message = text.to_string();
                message_count += 1;
            }
            Message::Close(_) => {
                println!("Server closed the connection");
                break;
            }
            _ => {
                println!("Received non-text message: {message:?}");
            }
        }
    }

    // Make sure we received at least one message.
    assert!(message_count > 0, "Should have received at least one message");

    // Check if the last message matches the expected exit message.
    let expected_exit_message = r#"{"type":"exit","code":0,"error_message":null}"#;
    assert_eq!(
        last_text_message, expected_exit_message,
        "Last message should be an exit message with code 0"
    );

    // Close the connection if server hasn't done so already.
    let _ = write.send(Message::Close(None)).await;

    // We don't gracefully stop the server, just let the thread continue running
    // and it will be cleaned up when the process exits.
    println!("Test completed successfully with {message_count} messages received");
}

#[actix_rt::test]
async fn test_cmd_builtin_function_install_all() {
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

    // Create a basic request just to test if the route is defined.
    let req = test::TestRequest::get().uri("/ws/builtin-function").to_request();

    // Execute the request but don't check for success. This just verifies
    // that the route exists and the handler is called.
    let resp = test::call_service(&app, req).await;

    println!("Response status: {:?}", resp.status());

    // Instead of asserting success which requires WebSocket setup, we'll
    // just verify that the endpoint exists by checking the response
    // is not a 404.
    assert_ne!(resp.status().as_u16(), 404, "Endpoint not found");

    // Note: A proper WebSocket test would require a more complex setup.
    // This test just verifies the route is registered correctly.
    println!("WebSocket endpoint /ws/builtin-function is registered and available");
}
