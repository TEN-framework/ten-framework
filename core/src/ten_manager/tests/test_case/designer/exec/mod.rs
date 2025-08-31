//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

//! This file contains integration tests for the exec_endpoint function
//!
//! exec_endpoint is a WebSocket endpoint used for:
//! 1. Direct execution of system commands (ExecCmd)
//! 2. Execution of scripts defined in manifest.json (RunScript)
//!
//! Test coverage:
//! - Command execution functionality
//! - Script execution functionality
//! - Error handling

use std::{collections::HashMap, sync::Arc};

use actix_web::web;
use futures_util::{SinkExt, StreamExt};
use ten_manager::{
    designer::{
        exec::{exec_endpoint, InboundMsg},
        storage::in_memory::TmanStorageInMemory,
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};

use crate::test_case::common::{builtin_server::start_test_server, mock::inject_all_pkgs_for_mock};

/// Test system command execution via WebSocket
///
/// This test verifies:
/// 1. Whether WebSocket connection can be established successfully
/// 2. Whether ExecCmd messages can be sent
/// 3. Whether command execution output can be received
/// 4. Whether command completion signals are handled correctly
#[actix_rt::test]
async fn test_exec_endpoint_command_execution() {
    // Start test server
    // start_test_server is a helper function that starts a real server
    let server_addr = start_test_server("/ws/exec", || web::get().to(exec_endpoint)).await;
    println!("📡 Test server started at: {server_addr}");

    // Establish WebSocket client connection
    let ws_url = format!("ws://{server_addr}/ws/exec");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("🔗 WebSocket connection established");

    // Split into read/write streams so we can send and receive messages
    // simultaneously
    let (mut write, mut read) = ws_stream.split();

    // Prepare command message to execute
    // We use 'echo' command because it's simple and available on all systems
    // Use cross-platform temporary directory
    let temp_dir = std::env::temp_dir();
    let exec_cmd_msg = InboundMsg::ExecCmd {
        base_dir: temp_dir.to_string_lossy().to_string(), // Working directory
        cmd: "echo 'Hello from exec test'".to_string(),   // Command to execute
        // Standard output not treated as log
        stdout_is_log: false,
        stderr_is_log: false, // Error output not treated as log
    };

    // Serialize message to JSON
    let json_msg = serde_json::to_string(&exec_cmd_msg).unwrap();
    println!("📤 Sending ExecCmd message: {json_msg}");

    // Send message via WebSocket
    write.send(Message::Text(json_msg.into())).await.unwrap();

    // Collect server responses
    let mut received_output = false;
    let mut received_exit = false;
    let mut message_count = 0;
    let max_wait = 20; // Maximum number of messages to wait for

    while let Some(msg_result) = read.next().await {
        match msg_result {
            Ok(Message::Text(text)) => {
                message_count += 1;
                println!("📥 Received message #{message_count}: {text}");

                // Verify response message is not empty
                assert!(!text.is_empty(), "Response should not be empty");

                // Check if we received expected output type
                if text.contains("stdout_normal") && text.contains("Hello from exec test") {
                    received_output = true;
                    println!("✅ Received expected command output");
                }

                if text.contains("exit") {
                    received_exit = true;
                    println!("✅ Received command exit signal");
                }

                // If we received both output and exit signal, command execution
                // is complete
                if received_output && received_exit {
                    break;
                }

                // Prevent infinite loop, set reasonable message count limit
                if message_count >= max_wait {
                    break;
                }
            }
            Ok(Message::Close(_)) => {
                println!("🔚 Server closed the connection");
                break;
            }
            Ok(_) => {
                // Ignore other message types (like Ping/Pong)
            }
            Err(e) => {
                panic!("❌ WebSocket error: {e}");
            }
        }
        // Add a small delay to ensure all async messages are received
        tokio::time::sleep(std::time::Duration::from_millis(20)).await;
    }

    // Verify we received at least one message
    assert!(message_count > 0, "Should receive at least one message");

    // Verify we received command output
    assert!(received_output, "Should receive command output");

    // Verify we received command completion signal
    assert!(received_exit, "Should receive command exit signal");

    println!("✅ Command execution test passed!");
}

/// Test RunScript functionality
///
/// This test verifies:
/// 1. Whether scripts can be correctly found from manifest.json scripts field
/// 2. Whether script commands defined in manifest can be executed
/// 3. Difference between RunScript and ExecCmd: RunScript uses script name,
///    ExecCmd uses direct command
/// 4. Correct usage of pkgs_cache
/// 5. Real WebSocket communication and script execution validation
#[actix_rt::test]
async fn test_exec_endpoint_run_script() {
    // Create test manifest.json content containing scripts
    let app_manifest_with_scripts = r#"{
  "type": "app",
  "name": "test_app_with_scripts",
  "version": "1.0.0",
  "dependencies": [
    {
      "type": "system",
      "name": "ten_runtime",
      "version": "0.2.0"
    }
  ],
  "scripts": {
    "test": "echo 'Running test script from manifest'",
    "build": "echo 'Building project'",
    "dev": "echo 'Starting development server'"
  }
}"#;

    // Set up test data: create pkgs_cache containing scripts
    let mut pkgs_cache = HashMap::new();
    let mut graphs_cache = HashMap::new();
    // Use cross-platform temporary directory
    let temp_dir = std::env::temp_dir();
    let test_base_dir = temp_dir.to_string_lossy();

    // Prepare package information - (base_dir, manifest.json, property.json)
    let all_pkgs_json = vec![(
        test_base_dir.to_string(),
        app_manifest_with_scripts.to_string(),
        "{}".to_string(), // Empty property.json
    )];

    // Use mock function to populate pkgs_cache
    inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json)
        .await
        .expect("Failed to inject test packages");

    println!("📦 Successfully injected test packages into pkgs_cache");

    // Create DesignerState with correct pkgs_cache
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(pkgs_cache),
        graphs_cache: tokio::sync::RwLock::new(graphs_cache),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Start a real test server with custom DesignerState
    let (addr_tx, addr_rx) = tokio::sync::oneshot::channel();
    let designer_state_clone = designer_state.clone();

    std::thread::spawn(move || {
        let rt = tokio::runtime::Runtime::new().unwrap();
        rt.block_on(async {
            // Start the server with custom DesignerState
            let server = actix_web::HttpServer::new(move || {
                actix_web::App::new()
                    .app_data(web::Data::new(designer_state_clone.clone()))
                    .route("/ws/exec", web::get().to(exec_endpoint))
            })
            .bind("127.0.0.1:0")
            .unwrap();

            // Get the actual server address
            let server_addr = server.addrs()[0];
            let _ = addr_tx.send(server_addr);

            // Run the server
            server.run().await.unwrap();
        });
    });

    // Wait for the server to start and get the address
    let server_addr = addr_rx.await.unwrap();

    // Give server time to fully start
    tokio::time::sleep(std::time::Duration::from_millis(1000)).await;

    println!("📡 Test server started at: {server_addr}");

    // Establish WebSocket client connection
    let ws_url = format!("ws://{server_addr}/ws/exec");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("🔗 WebSocket connection established");

    // Split into read/write streams
    let (mut write, mut read) = ws_stream.split();

    // Send RunScript message - Note: name field is script name, not command!
    let run_script_msg = InboundMsg::RunScript {
        base_dir: test_base_dir.to_string(), /* Must match base_dir in
                                              * pkgs_cache */
        name: "test".to_string(), /* Script name, corresponds to scripts.test
                                   * in manifest.json */
        stdout_is_log: false,
        stderr_is_log: false,
    };

    let json_msg = serde_json::to_string(&run_script_msg).unwrap();
    println!("📤 Sending RunScript message: {json_msg}");
    println!("📝 This should execute: echo 'Running test script from manifest'");

    // Send message via WebSocket
    write.send(Message::Text(json_msg.into())).await.unwrap();

    // Collect server responses
    let mut message_count = 0;
    let mut received_output = false;
    let mut received_exit = false;

    // Read response messages until connection closes or reasonable message
    // limit is reached
    while let Some(msg_result) = read.next().await {
        match msg_result {
            Ok(Message::Text(text)) => {
                message_count += 1;
                println!("📥 Received message #{message_count}: {text}");

                // Verify response message is not empty
                assert!(!text.is_empty(), "Response should not be empty");

                // Check if we received expected script output
                if text.contains("stdout_normal")
                    && text.contains("Running test script from manifest")
                {
                    received_output = true;
                    println!("✅ Received expected script output");
                }

                if text.contains("exit") {
                    received_exit = true;
                    println!("✅ Received script exit signal");
                }

                // If we received both output and exit signal, script execution
                // is complete
                if received_output && received_exit {
                    break;
                }

                // Prevent infinite loop, set reasonable message count limit
                if message_count >= 10 {
                    break;
                }
            }
            Ok(Message::Close(_)) => {
                println!("🔚 Server closed the connection");
                break;
            }
            Ok(_) => {
                // Ignore other message types (like Ping/Pong)
            }
            Err(e) => {
                panic!("❌ WebSocket error: {e}");
            }
        }
    }

    // Verify we received at least one message
    assert!(message_count > 0, "Should receive at least one message");

    // Verify we received script output
    assert!(received_output, "Should receive script output");

    // Verify we received script completion signal
    assert!(received_exit, "Should receive script exit signal");

    println!("✅ RunScript execution test passed!");
    println!("📦 Successfully validated pkgs_cache injection with scripts:");
    println!("   - test: echo 'Running test script from manifest'");
    println!("   - build: echo 'Building project'");
    println!("   - dev: echo 'Starting development server'");
    println!("🎯 Key difference: RunScript.name='test' vs ExecCmd.cmd='echo ...'");
    println!("🚀 RunScript successfully executed and validated real WebSocket communication!");
}

/// Test execution of invalid commands
///
/// This test verifies:
/// 1. How the system handles non-existent commands
/// 2. Whether error information is returned correctly
/// 3. Whether error handling meets expectations
#[actix_rt::test]
async fn test_exec_endpoint_invalid_command() {
    // Start test server
    let server_addr = start_test_server("/ws/exec", || web::get().to(exec_endpoint)).await;
    println!("📡 Test server started at: {server_addr}");

    // Establish WebSocket connection
    let ws_url = format!("ws://{server_addr}/ws/exec");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("🔗 WebSocket connection established");

    let (mut write, mut read) = ws_stream.split();

    // Send an invalid command
    // Use cross-platform temporary directory
    let temp_dir = std::env::temp_dir();
    let exec_cmd_msg = InboundMsg::ExecCmd {
        base_dir: temp_dir.to_string_lossy().to_string(),
        cmd: "this_command_does_not_exist_12345".to_string(), /* Non-existent
                                                               * command */
        stdout_is_log: false,
        stderr_is_log: false,
    };

    let json_msg = serde_json::to_string(&exec_cmd_msg).unwrap();
    println!("📤 Sending invalid command: {json_msg}");

    write.send(Message::Text(json_msg.into())).await.unwrap();

    // Collect responses
    let mut message_count = 0;
    let mut received_error_or_exit = false;

    while let Some(msg_result) = read.next().await {
        match msg_result {
            Ok(Message::Text(text)) => {
                message_count += 1;
                println!("📥 Received message #{message_count}: {text}");

                // Check if we received error information or non-zero exit code
                if text.contains("error")
                    || text.contains("stderr")
                    || (text.contains("exit") && !text.contains("\"code\":0"))
                {
                    received_error_or_exit = true;
                    println!("✅ Received expected error response");
                }

                if received_error_or_exit || message_count >= 5 {
                    break;
                }
            }
            Ok(Message::Close(_)) => {
                println!("🔚 Server closed the connection");
                break;
            }
            Ok(_) => {}
            Err(e) => {
                panic!("❌ WebSocket error: {e}");
            }
        }
    }

    // Verify we received appropriate error response
    assert!(message_count > 0, "Should receive at least one message");
    assert!(
        received_error_or_exit,
        "Should receive error or non-zero exit code for invalid command"
    );

    println!("✅ Invalid command test passed!");
}

/// Test sending invalid JSON message
///
/// This test verifies:
/// 1. How server handles malformed JSON
/// 2. Whether parse errors are returned correctly
/// 3. Whether connection is closed appropriately
#[actix_rt::test]
async fn test_exec_endpoint_invalid_json() {
    let server_addr = start_test_server("/ws/exec", || web::get().to(exec_endpoint)).await;
    println!("📡 Test server started at: {server_addr}");

    let ws_url = format!("ws://{server_addr}/ws/exec");
    let (ws_stream, _) = connect_async(ws_url).await.unwrap();
    println!("🔗 WebSocket connection established");

    let (mut write, mut read) = ws_stream.split();

    // Send invalid JSON
    let invalid_json = "{ invalid json here }";
    println!("📤 Sending invalid JSON: {invalid_json}");

    write.send(Message::Text(invalid_json.into())).await.unwrap();

    // Wait for response
    let mut message_count = 0;
    let mut received_error = false;

    while let Some(msg_result) = read.next().await {
        match msg_result {
            Ok(Message::Text(text)) => {
                message_count += 1;
                println!("📥 Received message #{message_count}: {text}");

                // Check if we received error message
                if text.contains("error") || text.contains("Failed to parse") {
                    received_error = true;
                    println!("✅ Received expected JSON parse error");
                }

                if received_error || message_count >= 3 {
                    break;
                }
            }
            Ok(Message::Close(_)) => {
                println!("🔚 Server closed the connection (expected for invalid JSON)");
                break;
            }
            Ok(_) => {}
            Err(e) => {
                // Connection might close due to invalid JSON, this is expected
                // behavior
                println!("📡 Connection closed due to invalid JSON: {e}");
                break;
            }
        }
    }

    // For invalid JSON, we expect to receive error message or connection to be
    // closed Either case is reasonable error handling
    assert!(
        received_error || message_count == 0,
        "Should receive error message or connection should be closed for invalid JSON"
    );

    println!("✅ Invalid JSON test passed!");
}

// Note: RunScript functionality tests require manifest.json files and
// corresponding test data Such tests usually need more complex test environment
// setup, can be extended later
