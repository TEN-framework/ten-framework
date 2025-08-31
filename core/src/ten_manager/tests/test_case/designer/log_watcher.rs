//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs::File, io::Write, path::Path, time::Duration};

use actix_web::web;
use futures_util::{SinkExt, StreamExt};
use serde_json::json;
use tempfile::tempdir;
use ten_manager::{designer::log_watcher::log_watcher_endpoint, log::LogLineInfo};
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};

use crate::test_case::common::{builtin_server::start_test_server, fs::sync_to_disk};

#[actix_rt::test]
async fn test_ws_log_watcher_endpoint() {
    // Create a temporary directory to simulate an app base dir.
    let temp_dir = tempdir().unwrap();
    let app_dir = temp_dir.path();

    // Create a app.log file in the temp dir.
    let log_file_path = app_dir.join("app.log");
    eprintln!("Log file path: {}", log_file_path.display());
    create_property_json(app_dir, &log_file_path);

    // Create an empty log file.
    create_empty_log_file(&log_file_path);

    // Start the WebSocket server and get its address.
    let server_addr =
        start_test_server("/ws/log-watcher", || web::get().to(log_watcher_endpoint)).await;
    println!("Server started at: {server_addr}");

    // Connect WebSocket client to the server with the app_base_dir parameter.
    let ws_url = format!("ws://{server_addr}/ws/log-watcher");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("WebSocket connection established");

    // Split the WebSocket stream.
    let (mut write, mut read) = ws_stream.split();

    // Wait for the "ready" message from the server.
    let mut ready_received = false;
    while let Some(msg) = read.next().await {
        let msg = msg.unwrap();
        if msg.is_text() {
            let text = msg.to_text().unwrap();
            println!("({}) Received: {text}", log_file_path.display());
            if text.contains("Ready to receive app_base_dir") {
                ready_received = true;
                break;
            }
        }
    }
    assert!(ready_received, "({}) Didn't receive ready message", log_file_path.display());

    // Send the app_base_dir to the server.
    let app_base_dir_msg = json!({
        "type": "set_app_base_dir",
        "app_base_dir": app_dir.to_string_lossy().to_string()
    });
    write.send(Message::Text(app_base_dir_msg.to_string().into())).await.unwrap();
    println!("({}) Sent app_base_dir message", log_file_path.display());

    // Wait for the info message about starting the watcher.
    let mut received_start_msg = false;
    while let Some(msg) = read.next().await {
        let msg = msg.unwrap();
        if msg.is_text() {
            let text = msg.to_text().unwrap();
            println!("({}) Received: {text}", log_file_path.display());
            if text.contains("Started watching log file") {
                received_start_msg = true;
                break;
            }
        }
    }
    assert!(received_start_msg, "({}) Didn't receive start message", log_file_path.display());

    // Now append to the log file.
    let test_content = "Test log message\n";
    append_to_log_file(&log_file_path, test_content);
    eprintln!("({}) Appended to log file: {}", log_file_path.display(), test_content);

    // Check if we receive the content - with timeout of 10 seconds.
    let mut received_content = false;
    if let Ok(Some(msg)) = tokio::time::timeout(Duration::from_secs(10), read.next()).await {
        let msg = msg.unwrap();
        if msg.is_text() {
            let text = msg.to_text().unwrap();
            println!("({}) Received text: {text}", log_file_path.display());

            // Try to parse the JSON response.
            if let Ok(log_line_info) = serde_json::from_str::<LogLineInfo>(text) {
                if log_line_info.line.contains(test_content.trim()) {
                    received_content = true;
                }
            } else if text.contains(test_content.trim()) {
                // Fallback to direct text comparison (for backward
                // compatibility).
                received_content = true;
            }
        }
    }
    assert!(received_content, "({}) Didn't receive log content", log_file_path.display());

    // Send stop message.
    let stop_msg = r#"{"type":"stop"}"#;
    write.send(Message::Text(stop_msg.into())).await.unwrap();
    println!("({}) Sent stop message", log_file_path.display());

    // Wait for connection to close or stop confirmation.
    let mut received_stop = false;
    while let Ok(Some(msg)) = tokio::time::timeout(Duration::from_secs(5), read.next()).await {
        let msg = msg.unwrap();
        if msg.is_text() {
            let text = msg.to_text().unwrap();
            println!("({}) Received: {text}", log_file_path.display());
            if text.contains("Stopped watching log file") {
                received_stop = true;
                break;
            }
        }
    }
    assert!(received_stop, "({}) Didn't receive stop confirmation", log_file_path.display());

    // Clean up.
    temp_dir.close().unwrap();
}

fn create_property_json(app_dir: &Path, log_file_path: &Path) {
    let property_json = format!(
        r#"{{
            "ten": {{
                "log": {{
                    "handlers": [
                        {{
                            "matchers": [
                                {{
                                    "level": "info"
                                }}
                            ],
                            "formatter": {{
                                "type": "json",
                                "colored": false
                            }},
                            "emitter": {{
                                "type": "file",
                                "config": {{
                                    "path": "{}"
                                }}
                            }}
                        }}
                    ]
                }}
            }}
        }}"#,
        log_file_path.file_name().unwrap().to_string_lossy()
    );

    let property_path = app_dir.join("property.json");
    let mut file = File::create(property_path).unwrap();
    file.write_all(property_json.as_bytes()).unwrap();
}

fn create_empty_log_file(path: &Path) {
    File::create(path).unwrap();
}

fn append_to_log_file(path: &Path, content: &str) {
    let mut file = std::fs::OpenOptions::new().append(true).open(path).unwrap();
    file.write_all(content.as_bytes()).unwrap();
    file.flush().unwrap();
    sync_to_disk(&file).unwrap();
}
