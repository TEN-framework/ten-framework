//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    sync::{mpsc, Arc},
    thread,
};

use actix::{fut, AsyncContext};
use actix_web_actors::ws::WebsocketContext;

use crate::{
    cmd::cmd_install::InstallCommand,
    designer::{
        builtin_function::{BuiltinFunctionOutput, WsBuiltinFunction},
        storage::in_memory::TmanStorageInMemory,
    },
    home::config::TmanConfig,
    output::{channel::TmanOutputChannel, TmanOutput},
};

pub fn run_installation(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    install_command: InstallCommand,
    ctx: &mut WebsocketContext<WsBuiltinFunction>,
) {
    let addr = ctx.address();

    // Create a channel for cross-thread communication.
    let (sender, receiver) = mpsc::channel();

    let output_channel = Arc::new(Box::new(TmanOutputChannel {
        sender: sender.clone(),
    }) as Box<dyn TmanOutput>);

    // Run the installation process in a new thread.
    //
    // Since `cmd_install::execute_cmd()` is an operation that takes a long time
    // (possibly several seconds or even minutes), although `execute_cmd()` is
    // an async function and contains many await points, these await points
    // may not necessarily enter the Poll::Pending state, making
    // `execute_cmd()` likely to behave like a synchronous function call
    // throughout its execution. In the actix-web architecture, each worker
    // is a single-threaded structure, so when `execute_cmd()` blocks this
    // single thread and cannot enter Pending due to await to yield control
    // to the underlying event loop to handle tasks/messages, the websocket
    // client messages (i.e., logs) sent during the `execute_cmd()`
    // process will basically not be sent to the websocket client, and will only
    // be sent all at once after `execute_cmd()` finishes. This results in a
    // poor user experience, so the time-consuming `execute_cmd()` is executed
    // in a new thread, where a simple async task can be used to receive
    // messages/logs sent through the channel. After the new thread
    // completes `execute_cmd()`, it sends the result back to the original
    // thread through the channel, and the original thread then sends the result
    // to the websocket client via websocket.
    thread::spawn(move || {
        // Create a new Tokio runtime to execute asynchronous code.
        //
        // Since in the whole execution flow of `cmd_install::execute_cmd()`
        // does not use `tokio::spawn()` to start other tokio tasks, there is no
        // need to use a multi-thread runtime. A single-thread runtime created
        // using `new_current_thread` can be used.
        let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap();

        // Execute the installation in the new runtime.
        //
        // Because `block_on` will block the current _new_ thread, and the async
        // block received by `block_on` will only execute on the current _new_
        // thread, therefore, resources within `cmd_install::execute_cmd()` that
        // do not have the `Send+Sync` trait, such as `clingo`, can safely run
        // inside `block_on`.
        let result = rt.block_on(async {
            crate::cmd::cmd_install::execute_cmd(
                tman_config,
                tman_storage_in_memory,
                install_command,
                output_channel,
            )
            .await
        });

        // Send the completion status to the main thread (an actix worker
        // thread).
        let exit_code = if result.is_ok() { 0 } else { -1 };
        let error_message = if let Err(err) = result { Some(err.to_string()) } else { None };

        let _ = sender.send(format!("EXIT:{}:{}", exit_code, error_message.unwrap_or_default()));
    });

    // Start a local task in the actix worker thread to listen to the message
    // channel.
    let addr_clone = addr.clone();

    // Use actix's `fut::wrap_future` to convert a standard `Future` to an
    // `ActorFuture`.
    ctx.spawn(fut::wrap_future::<_, WsBuiltinFunction>(async move {
        // Use a loop to poll the receiver.
        let mut continue_running = true;

        while continue_running {
            match receiver.try_recv() {
                Ok(msg) => {
                    if msg.starts_with("EXIT:") {
                        // Parse the exit status.
                        let parts: Vec<&str> = msg.splitn(3, ':').collect();
                        if parts.len() >= 2 {
                            let exit_code = parts[1].parse::<i32>().unwrap_or(-1);
                            let error_message = if parts.len() > 2 && !parts[2].is_empty() {
                                Some(parts[2].to_string())
                            } else {
                                None
                            };

                            // Send the exit message.
                            addr_clone.do_send(BuiltinFunctionOutput::Exit {
                                exit_code,
                                error_message,
                            });

                            // Exit the loop.
                            continue_running = false;
                        }
                    } else if msg.starts_with("normal_line:") {
                        // Parse and send normal logs.
                        let content = msg.replacen("normal_line:", "", 1);

                        addr_clone.do_send(BuiltinFunctionOutput::NormalLine(content));
                    } else if msg.starts_with("normal_partial:") {
                        // Parse and send normal partial logs.
                        let content = msg.replacen("normal_partial:", "", 1);

                        addr_clone.do_send(BuiltinFunctionOutput::NormalPartial(content));
                    } else if msg.starts_with("error_line:") {
                        // Parse and send error line logs.
                        let content = msg.replacen("error_line:", "", 1);

                        addr_clone.do_send(BuiltinFunctionOutput::ErrorLine(content));
                    } else if msg.starts_with("error_partial:") {
                        // Parse and send error partial logs.
                        let content = msg.replacen("error_partial:", "", 1);

                        addr_clone.do_send(BuiltinFunctionOutput::ErrorPartial(content));
                    }
                }
                Err(mpsc::TryRecvError::Empty) => {
                    // No message, temporarily yield control.
                    tokio::task::yield_now().await;
                }
                Err(mpsc::TryRecvError::Disconnected) => {
                    // The sender has disconnected, exit the loop.
                    continue_running = false;
                }
            }
        }
    }));
}
