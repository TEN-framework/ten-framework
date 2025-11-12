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
use tokio::time::sleep;
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

    // Wait for the initial welcome message.
    let mut message_count = 0;
    let mut has_welcome_message = false;

    // First, read the welcome messages.
    while let Some(Ok(message)) = read.next().await {
        match message {
            Message::Text(text) => {
                println!("Received initial message #{}: {}", message_count + 1, text);
                message_count += 1;
                has_welcome_message = true;

                // After receiving a few welcome messages, break to proceed with
                // the test.
                if message_count >= 3 {
                    break;
                }
            }
            Message::Binary(bin) => {
                println!("Received binary data with length: {}", bin.len());
                message_count += 1;
            }
            _ => {
                println!("Received other message type: {message:?}");
            }
        }
    }

    // Verify that we received the welcome messages.
    assert!(has_welcome_message, "Should have received welcome messages");

    // Send a command to the terminal.
    let command = "echo 'Hello from terminal test'\n";

    write.send(Message::Text(command.into())).await.unwrap();
    println!("Sent command: {command}");

    // Wait a moment for the command to execute.
    sleep(Duration::from_millis(500)).await;

    // Send a resize message.
    let resize_msg = r#"{"type":"resize","cols":100,"rows":30}"#;

    write.send(Message::Text(resize_msg.into())).await.unwrap();
    println!("Sent resize message: {resize_msg}");

    // Wait a moment for the resize to take effect.
    sleep(Duration::from_millis(300)).await;

    // Send an exit command to close the terminal properly.
    #[cfg(target_os = "windows")]
    let exit_command = "exit\r\n";
    #[cfg(not(target_os = "windows"))]
    let exit_command = "exit\n";

    write.send(Message::Text(exit_command.into())).await.unwrap();
    println!("Sent exit command to close the terminal");

    // Read responses until we get an exit message or timeout.
    let mut got_exit_message = false;
    let mut response_count = 0;

    // Set a timeout for how long to wait for responses.
    let timeout = Duration::from_secs(3);
    let start_time = std::time::Instant::now();

    while let Some(Ok(message)) = read.next().await {
        // Check if we've exceeded the timeout.
        if start_time.elapsed() > timeout {
            println!("Timeout reached, breaking read loop");
            break;
        }

        match message {
            Message::Text(text) => {
                println!("Received response #{}: {}", response_count + 1, text);

                // Check if this is an exit message.
                if text.contains(r#""type":"exit"#) {
                    println!("Found exit message: {text}");
                    got_exit_message = true;

                    // Verify the exit message format.
                    let expected_exit_message_part = r#""type":"exit"#;
                    assert!(
                        text.contains(expected_exit_message_part),
                        "Exit message should contain exit code information"
                    );

                    break;
                }

                response_count += 1;
            }
            Message::Binary(bin) => {
                println!("Received binary data with length: {}", bin.len());
                response_count += 1;
            }
            Message::Close(_) => {
                println!("Server closed the connection");
                break;
            }
            _ => {
                println!("Received other message type: {message:?}");
            }
        }
    }

    // Verify that we got responses to our commands.
    assert!(response_count > 0, "Should have received responses to our commands");

    // Verify that we got an exit message or the connection was closed.
    assert!(got_exit_message, "Should have received an exit message");

    // Close the connection if the server hasn't done so already.
    let _ = write.send(Message::Close(None)).await;

    println!(
        "Test completed successfully with {message_count} initial messages and {response_count} \
         response messages"
    );
}
