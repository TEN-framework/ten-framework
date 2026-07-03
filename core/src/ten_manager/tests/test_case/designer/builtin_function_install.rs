//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use actix_web::web;
use futures_util::{SinkExt, StreamExt};
use ten_manager::designer::builtin_function::{builtin_function_endpoint, msg::InboundMsg};
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};

use crate::test_case::common::builtin_server::start_test_server;

#[actix_rt::test]
async fn test_ws_builtin_function_install() {
    // Start the WebSocket server and get its address.
    let server_addr =
        start_test_server("/ws/builtin-function", || web::get().to(builtin_function_endpoint))
            .await;
    println!("Server started at: {server_addr}");

    // Connect WebSocket client.
    let ws_url = format!("ws://{server_addr}/ws/builtin-function");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("WebSocket connection established");

    // Split the WebSocket stream.
    let (mut write, mut read) = ws_stream.split();

    // Prepare Install message.
    let install_msg = InboundMsg::Install {
        base_dir: "tests/test_data/cmd_builtin_function_install".to_string(),
        pkg_type: "extension".to_string(),
        pkg_name: "ext_b".to_string(),
        pkg_version: None,
    };
    let json_msg = serde_json::to_string(&install_msg).unwrap();

    // Send the message.
    write.send(Message::Text(json_msg.clone().into())).await.unwrap();
    println!("Sent Install message: {json_msg}");

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
