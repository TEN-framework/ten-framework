//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod cmd_run;
mod msg;
mod run_script;

// Re-export only the types that should be public API
use std::{process::Child, sync::Arc};

use actix::{Actor, AsyncContext, Handler, Message, StreamHandler};
use actix_web::{web, Error, HttpRequest, HttpResponse};
use actix_web_actors::ws;
use anyhow::{Context, Result};
use cmd_run::ShutdownSenders;
pub use msg::{InboundMsg, OutboundMsg};
use run_script::extract_command_from_manifest;

use crate::{designer::DesignerState, log::LogLineInfo};

// The output (stdout, stderr) and exit status from the child process.
#[derive(Message)]
#[rtype(result = "()")]
pub enum RunCmdOutput {
    StdOutNormal(String),
    StdOutLog(LogLineInfo),

    StdErrNormal(String),
    StdErrLog(LogLineInfo),

    Exit(i32),
}

/// `CmdParser` returns a tuple: the 1st element is the command string, the 2nd
/// is an optional working directory, and the 3rd and 4th are booleans
/// indicating if stdout and stderr are log content.
pub type CmdParser = Box<
    dyn Fn(
            &str,
        ) -> std::pin::Pin<
            Box<
                dyn std::future::Future<Output = Result<(String, Option<String>, bool, bool)>>
                    + Send,
            >,
        > + Send
        + Sync,
>;

pub struct WsRunCmd {
    child: Option<Child>,
    cmd_parser: CmdParser,
    working_directory: Option<String>,
    shutdown_senders: Option<ShutdownSenders>,
    stdout_is_log: bool,
    stderr_is_log: bool,
}

impl WsRunCmd {
    pub fn new(cmd_parser: CmdParser) -> Self {
        Self {
            child: None,
            cmd_parser,
            working_directory: None,
            shutdown_senders: None,
            stdout_is_log: false,
            stderr_is_log: false,
        }
    }
}

impl Actor for WsRunCmd {
    // Each actor runs within its own context and can receive and process
    // messages. This context provides various methods for interacting with the
    // actor, such as sending messages, closing connections, and more.
    type Context = ws::WebsocketContext<Self>;

    fn started(&mut self, _ctx: &mut Self::Context) {
        // We don't yet spawn the child command. We'll wait for the first
        // message from client that includes `base_dir` and `name`.
    }

    fn stopped(&mut self, _ctx: &mut Self::Context) {
        // Call our new cleanup method to properly terminate all threads.
        self.cleanup_threads();
    }
}

impl Handler<RunCmdOutput> for WsRunCmd {
    type Result = ();

    // Handles the output (stderr, stdout) and exit status from the child
    // process.
    fn handle(&mut self, msg: RunCmdOutput, ctx: &mut Self::Context) -> Self::Result {
        match msg {
            RunCmdOutput::StdOutNormal(line) => {
                // Send the line to the client.
                let msg_out = OutboundMsg::StdOutNormal {
                    data: line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            RunCmdOutput::StdOutLog(log_line) => {
                let msg_out = OutboundMsg::StdOutLog {
                    data: log_line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            RunCmdOutput::StdErrNormal(line) => {
                let msg_out = OutboundMsg::StdErrNormal {
                    data: line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            RunCmdOutput::StdErrLog(log_line) => {
                let msg_out = OutboundMsg::StdErrLog {
                    data: log_line,
                };
                let out_str = serde_json::to_string(&msg_out).unwrap();

                // Sends a text message to the WebSocket client.
                ctx.text(out_str);
            }
            RunCmdOutput::Exit(code) => {
                // Send it to the client.
                let msg_out = OutboundMsg::Exit {
                    code,
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

impl StreamHandler<Result<ws::Message, ws::ProtocolError>> for WsRunCmd {
    // Handles messages from WebSocket clients, including text messages, Ping,
    // Close, and more.
    fn handle(&mut self, item: Result<ws::Message, ws::ProtocolError>, ctx: &mut Self::Context) {
        match item {
            Ok(ws::Message::Text(text)) => {
                println!("Received text: {text}");

                let fut = (self.cmd_parser)(&text);
                let actor_addr = ctx.address();

                actix::spawn(async move {
                    match fut.await {
                        Ok((cmd, working_directory, stdout_is_log, stderr_is_log)) => {
                            actor_addr.do_send(ProcessCommand {
                                cmd,
                                working_directory,
                                stdout_is_log,
                                stderr_is_log,
                            });
                        }
                        Err(e) => {
                            let err_out = OutboundMsg::Error {
                                msg: e.to_string(),
                            };
                            let out_str = serde_json::to_string(&err_out).unwrap();
                            actor_addr.do_send(SendText(out_str));
                            actor_addr.do_send(CloseConnection);
                        }
                    }
                });
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

// New message types for async communication.
#[derive(Message)]
#[rtype(result = "()")]
struct ProcessCommand {
    cmd: String,
    working_directory: Option<String>,
    stdout_is_log: bool,
    stderr_is_log: bool,
}

#[derive(Message)]
#[rtype(result = "()")]
struct SendText(String);

#[derive(Message)]
#[rtype(result = "()")]
struct CloseConnection;

impl Handler<SendText> for WsRunCmd {
    type Result = ();

    fn handle(&mut self, msg: SendText, ctx: &mut Self::Context) -> Self::Result {
        ctx.text(msg.0);
    }
}

impl Handler<CloseConnection> for WsRunCmd {
    type Result = ();

    fn handle(&mut self, _: CloseConnection, ctx: &mut Self::Context) -> Self::Result {
        ctx.close(None);
    }
}

impl Handler<ProcessCommand> for WsRunCmd {
    type Result = ();

    fn handle(&mut self, msg: ProcessCommand, ctx: &mut Self::Context) -> Self::Result {
        if let Some(dir) = msg.working_directory {
            self.working_directory = Some(dir);
        }

        self.stdout_is_log = msg.stdout_is_log;
        self.stderr_is_log = msg.stderr_is_log;

        self.cmd_run(&msg.cmd, ctx);
    }
}

pub async fn exec_endpoint(
    req: HttpRequest,
    stream: web::Payload,
    state: web::Data<Arc<DesignerState>>,
) -> Result<HttpResponse, Error> {
    let state_clone = state.get_ref().clone();

    // The client connects to the `run_app` route via WebSocket, creating an
    // instance of the `WsRunApp` actor.

    let default_parser: CmdParser = Box::new(move |text: &str| {
        let state_clone_inner = state_clone.clone();
        let text_owned = text.to_owned();

        Box::pin(async move {
            // Attempt to parse the JSON text from client.
            let inbound = serde_json::from_str::<InboundMsg>(&text_owned)
                .with_context(|| format!("Failed to parse {text_owned} into JSON"))?;

            match inbound {
                InboundMsg::ExecCmd {
                    base_dir,
                    cmd,
                    stdout_is_log,
                    stderr_is_log,
                } => Ok((cmd, Some(base_dir), stdout_is_log, stderr_is_log)),
                InboundMsg::RunScript {
                    base_dir,
                    name,
                    stdout_is_log,
                    stderr_is_log,
                } => {
                    let cmd =
                        extract_command_from_manifest(&base_dir, &name, state_clone_inner).await?;
                    Ok((cmd, Some(base_dir), stdout_is_log, stderr_is_log))
                }
            }
        })
    });

    ws::start(WsRunCmd::new(default_parser), &req, stream)
}
