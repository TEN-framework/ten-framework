//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::time::Duration;

use actix_web::web;
use futures_util::{SinkExt, StreamExt};
use ten_manager::designer::terminal::ws_terminal_endpoint;
use tokio::time::timeout;
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};

use crate::test_case::common::builtin_server::start_test_server;

#[actix_rt::test]
async fn test_ws_terminal_endpoint() {
    // Start the WebSocket server and get its address.
    let server_addr =
        start_test_server("/ws/terminal", || web::get().to(ws_terminal_endpoint)).await;
    println!("Server started at: {server_addr}");

    // Connect WebSocket client to the server.
    // Use the current directory as the terminal path.
    let ws_url = format!("ws://{}/ws/terminal?path={}", server_addr, ".");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("WebSocket connection established");

    // Split the WebSocket stream.
    let (mut write, mut read) = ws_stream.split();

    // Wait for the complete welcome message so that it cannot be mistaken for
    // command output later in the test.
    let mut message_count = 0;
    let got_complete_welcome = timeout(Duration::from_secs(10), async {
        while let Some(message) = read.next().await {
            match message.expect("Failed to read initial WebSocket message") {
                Message::Text(text) => {
                    println!(
                        "Received initial message #{}: {}",
                        message_count + 1,
                        text
                    );
                    message_count += 1;

                    if text.contains("Enjoy your journey!") {
                        return true;
                    }
                }
                Message::Binary(bin) => {
                    println!("Received initial binary data with length: {}", bin.len());
                    message_count += 1;
                }
                Message::Close(_) => return false,
                message => {
                    println!("Received other initial message type: {message:?}");
                }
            }
        }

        false
    })
    .await
    .expect("Timed out waiting for the complete welcome message");

    assert!(
        got_complete_welcome,
        "Should have received the complete welcome message"
    );

    // Send a command to the terminal.
    let command = "echo 'Hello from terminal test'\n";

    write.send(Message::Text(command.into())).await.unwrap();
    println!("Sent command: {command}");

    // Wait for observable command output instead of relying on a fixed delay.
    let mut command_output = String::new();
    let got_command_output = timeout(Duration::from_secs(10), async {
        while let Some(message) = read.next().await {
            match message.expect("Failed to read command output") {
                Message::Text(text) => command_output.push_str(&text),
                Message::Binary(bin) => {
                    command_output.push_str(&String::from_utf8_lossy(&bin));
                }
                Message::Close(_) => return false,
                _ => {}
            }

            if command_output.matches("Hello from terminal test").count() >= 2 {
                return true;
            }
        }

        false
    })
    .await
    .expect("Timed out waiting for terminal command output");

    assert!(
        got_command_output,
        "Should have received terminal command output"
    );

    // Send a resize message.
    let resize_msg = r#"{"type":"resize","cols":100,"rows":30}"#;

    write.send(Message::Text(resize_msg.into())).await.unwrap();
    println!("Sent resize message: {resize_msg}");

    // Send an exit command to close the terminal properly.
    #[cfg(target_os = "windows")]
    let exit_command = "exit 0\r\n";
    #[cfg(not(target_os = "windows"))]
    let exit_command = "exit 0\n";

    write.send(Message::Text(exit_command.into())).await.unwrap();
    println!("Sent exit command to close the terminal");

    // Apply the timeout to the asynchronous read itself. Checking elapsed time
    // only after read.next() returns can wait forever when no message arrives.
    let exit_code = timeout(Duration::from_secs(10), async {
        while let Some(message) = read.next().await {
            match message.expect("Failed to read terminal exit message") {
                Message::Text(text) => {
                    println!("Received response: {text}");

                    if let Ok(exit_message) = serde_json::from_str::<serde_json::Value>(&text) {
                        if exit_message.get("type").and_then(|value| value.as_str()) == Some("exit")
                        {
                            return exit_message.get("code").and_then(|value| value.as_i64());
                        }
                    }
                }
                Message::Binary(bin) => {
                    println!("Received binary data with length: {}", bin.len());
                }
                Message::Close(_) => return None,
                message => println!("Received other message type: {message:?}"),
            }
        }

        None
    })
    .await
    .expect("Timed out waiting for the terminal exit message");

    assert_eq!(
        exit_code,
        Some(0),
        "Should have received a successful exit message"
    );

    // Close the connection if the server hasn't done so already.
    let _ = write.send(Message::Close(None)).await;

    println!("Test completed successfully with {message_count} initial messages");
}
