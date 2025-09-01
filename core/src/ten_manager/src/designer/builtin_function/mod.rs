//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod common;
mod install;
mod install_all;
pub mod msg;

use std::sync::Arc;

use actix::{Actor, Handler, Message, StreamHandler};
use actix_web::{web, Error, HttpRequest, HttpResponse};
use actix_web_actors::ws;
use anyhow::{Context, Result};
use msg::{InboundMsg, OutboundMsg};

use super::storage::in_memory::TmanStorageInMemory;
use crate::{designer::DesignerState, home::config::TmanConfig};

#[derive(Message)]
#[rtype(result = "()")]
pub enum BuiltinFunctionOutput {
    NormalLine(String),
    NormalPartial(String),
    ErrorLine(String),
    ErrorPartial(String),
    Exit { exit_code: i32, error_message: Option<String> },
}

enum BuiltinFunction {
    InstallAll { base_dir: String },
    Install { base_dir: String, pkg_type: String, pkg_name: String, pkg_version: Option<String> },
}

/// `BuiltinFunctionParser` returns a tuple: the 1st element is the command
/// string, and the 2nd is an optional working directory.
type BuiltinFunctionParser = Box<dyn Fn(&str) -> Result<BuiltinFunction> + Send + Sync>;

pub struct WsBuiltinFunction {
    builtin_function_parser: BuiltinFunctionParser,
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
}

impl WsBuiltinFunction {
    fn new(
        builtin_function_parser: BuiltinFunctionParser,
        tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
        tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    ) -> Self {
        Self {
            builtin_function_parser,
            tman_config,
            tman_storage_in_memory,
        }
    }
}

impl Actor for WsBuiltinFunction {
    // Each actor runs within its own context and can receive and process
    // messages. This context provides various methods for interacting with the
    // actor, such as sending messages, closing connections, and more.
    type Context = ws::WebsocketContext<Self>;

    fn started(&mut self, _ctx: &mut Self::Context) {}

    fn stopped(&mut self, _ctx: &mut Self::Context) {}
}

impl Handler<BuiltinFunctionOutput> for WsBuiltinFunction {
    type Result = ();

    // Handles the output and exit status from the builtin function.
    fn handle(&mut self, msg: BuiltinFunctionOutput, ctx: &mut Self::Context) -> Self::Result {
        match msg {
            BuiltinFunctionOutput::NormalLine(line) => {
                // Send the line to the client.
                let msg_out = OutboundMsg::NormalLine {
                    data: line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            BuiltinFunctionOutput::NormalPartial(line) => {
                // Send the line to the client.
                let msg_out = OutboundMsg::NormalPartial {
                    data: line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            BuiltinFunctionOutput::ErrorLine(line) => {
                let msg_out = OutboundMsg::ErrorLine {
                    data: line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            BuiltinFunctionOutput::ErrorPartial(line) => {
                let msg_out = OutboundMsg::ErrorPartial {
                    data: line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            BuiltinFunctionOutput::Exit {
                exit_code,
                error_message,
            } => {
                // Send it to the client.
                let msg_out = OutboundMsg::Exit {
                    code: exit_code,
                    error_message,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);

                // Close the WebSocket. Passing `None` as a parameter indicates
                // sending a default close frame (Close Frame) to the client and
                // closing the WebSocket connection.
                ctx.close(None);
            }
        }
    }
}

impl StreamHandler<Result<ws::Message, ws::ProtocolError>> for WsBuiltinFunction {
    // Handles messages from WebSocket clients, including text messages, Ping,
    // Close, and more.
    fn handle(&mut self, item: Result<ws::Message, ws::ProtocolError>, ctx: &mut Self::Context) {
        match item {
            Ok(ws::Message::Text(text)) => {
                println!("Received text: {text}");

                match (self.builtin_function_parser)(&text) {
                    Ok(builtin_function) => match builtin_function {
                        BuiltinFunction::InstallAll {
                            base_dir,
                        } => self.install_all(base_dir, ctx),
                        BuiltinFunction::Install {
                            base_dir,
                            pkg_type,
                            pkg_name,
                            pkg_version,
                        } => self.install(base_dir, pkg_type, pkg_name, pkg_version, ctx),
                    },
                    Err(e) => {
                        let err_out = OutboundMsg::ErrorLine {
                            data: e.to_string(),
                        };
                        let out_str = serde_json::to_string(&err_out).unwrap();
                        ctx.text(out_str);
                        ctx.close(None);
                    }
                }
            }
            Ok(ws::Message::Ping(msg)) => ctx.pong(&msg),
            Ok(ws::Message::Close(_)) => {
                ctx.close(None);
            }
            // Ignore other message types.
            _ => {}
        }
    }
}

pub async fn builtin_function_endpoint(
    req: HttpRequest,
    stream: web::Payload,
    state: web::Data<Arc<DesignerState>>,
) -> Result<HttpResponse, Error> {
    let tman_config = state.tman_config.clone();
    let tman_metadata = state.storage_in_memory.clone();

    let default_parser: BuiltinFunctionParser = Box::new(move |text: &str| {
        // Attempt to parse the JSON text from client.
        let inbound = serde_json::from_str::<InboundMsg>(text)
            .with_context(|| format!("Failed to parse {text} into JSON"))?;

        match inbound {
            InboundMsg::InstallAll {
                base_dir,
            } => Ok(BuiltinFunction::InstallAll {
                base_dir,
            }),
            InboundMsg::Install {
                base_dir,
                pkg_type,
                pkg_name,
                pkg_version,
            } => Ok(BuiltinFunction::Install {
                base_dir,
                pkg_type,
                pkg_name,
                pkg_version,
            }),
        }
    });

    ws::start(WsBuiltinFunction::new(default_parser, tman_config, tman_metadata), &req, stream)
}
