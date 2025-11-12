//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod pty_manager;

use std::{
    collections::HashMap,
    fmt::Display,
    sync::{Arc, Mutex},
};

use actix::{Actor, AsyncContext, Message, StreamHandler};
use actix_web::{web, Error, HttpRequest, HttpResponse};
use actix_web_actors::ws;
use pty_manager::PtyManager;
use serde::__private::from_utf8_lossy;
use serde_json::Value;

// The message to/from the pty.
#[derive(Message, Debug, Eq, PartialEq)]
#[rtype(result = "()")]
pub enum PtyMessage {
    Buffer(Vec<u8>),

    Exit(i32),
}

impl Display for PtyMessage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PtyMessage::Buffer(data) => {
                write!(f, "Buffer({:?})", from_utf8_lossy(data))
            }
            PtyMessage::Exit(code) => {
                write!(f, "Exit({code})")
            }
        }
    }
}

struct WsProcessor {
    pty: Arc<Mutex<PtyManager>>,
}

impl WsProcessor {
    fn new(path: String) -> Self {
        Self {
            pty: Arc::new(Mutex::new(PtyManager::new(path))),
        }
    }
}

impl actix::Handler<PtyMessage> for WsProcessor {
    type Result = ();

    fn handle(&mut self, msg: PtyMessage, ctx: &mut Self::Context) -> Self::Result {
        // Receive a message from actor (pty), and to interact with the
        // websocket client.

        match msg {
            PtyMessage::Buffer(data) => {
                ctx.binary(data);
            }
            PtyMessage::Exit(code) => {
                // Notify the WebSocket client that the process is exited.
                let exit_msg = serde_json::json!({
                    "type": "exit",
                    "code": code
                });
                ctx.text(exit_msg.to_string());

                // Close the current actor to close the websocket channel.
                ctx.close(None)
            }
        }
    }
}

impl Actor for WsProcessor {
    type Context = ws::WebsocketContext<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        ctx.text("  _______   ______   _   _ \r\n");
        ctx.text(" |__   __| |  ____| | \\ | |\r\n");
        ctx.text("    | |    | |__    |  \\| |\r\n");
        ctx.text("    | |    |  __|   | . ` |\r\n");
        ctx.text("    | |    | |____  | |\\  |\r\n");
        ctx.text("    |_|    |______| |_| \\_|\r\n");
        ctx.text("\r\n");
        ctx.text("Website: https://github.com/TEN-framework/\r\n");
        ctx.text("Documentation: https://doc.theten.ai/\r\n");
        ctx.text("Enjoy your journey!\r\n");

        // `rx` is a receiver which get data from pty.
        let rx = self.pty.lock().unwrap().start();

        let addr = ctx.address();

        std::thread::spawn(move || {
            loop {
                // Continue to get data from pty.
                match rx.recv() {
                    Ok(msg) => {
                        let should_exit = matches!(msg, PtyMessage::Exit(_));

                        // Re-send the data to the actor, the actual logic is
                        // done in `handle` function.
                        addr.do_send(msg);

                        // If its the Exit message, exit the loop.
                        if should_exit {
                            break;
                        }
                    }
                    Err(e) => {
                        println!("recv error: {e}");
                        break;
                    }
                }
            }
        });
    }
}

// Handle the data received from the websocket.
impl StreamHandler<Result<ws::Message, ws::ProtocolError>> for WsProcessor {
    fn handle(&mut self, msg: Result<ws::Message, ws::ProtocolError>, ctx: &mut Self::Context) {
        match msg {
            Ok(ws::Message::Ping(msg)) => ctx.pong(&msg),
            Ok(ws::Message::Text(text)) => {
                println!("Received text: {text}");

                // Try to parse the received message as a json.
                if let Ok(json) = serde_json::from_str::<Value>(&text) {
                    if let Some(message_type) = json.get("type").and_then(|v| v.as_str()) {
                        match message_type {
                            "resize" => {
                                if let (Some(cols), Some(rows)) = (
                                    json.get("cols").and_then(|v| v.as_u64()),
                                    json.get("rows").and_then(|v| v.as_u64()),
                                ) {
                                    let cols = cols as u16;
                                    let rows = rows as u16;
                                    self.pty.lock().unwrap().resize_pty(cols, rows).unwrap_or_else(
                                        |_| {
                                            println!(
                                                "Failed to resize PTY to cols: {cols}, rows: \
                                                 {rows}"
                                            )
                                        },
                                    );
                                }
                            }
                            _ => {
                                panic!("Should not happen.");
                            }
                        }
                    } else {
                        // If it's not a control message, treat it as user
                        // input.
                        self.pty.lock().unwrap().write_to_pty(&text).unwrap_or_else(|_| {
                            println!("Failed to write to PTY with data: {text}")
                        });
                    }
                } else {
                    // If it's not JSON, treat it as user input.
                    self.pty
                        .lock()
                        .unwrap()
                        .write_to_pty(&text)
                        .unwrap_or_else(|_| println!("Failed to write to PTY with data: {text}"));
                }
            }
            Ok(ws::Message::Binary(bin)) => ctx.binary(bin),
            _ => (),
        }
    }
}

pub async fn ws_terminal_endpoint(
    req: HttpRequest,
    stream: web::Payload,
) -> Result<HttpResponse, Error> {
    // Extract 'path' from query parameters.
    let query = req.query_string();
    let params: HashMap<_, _> =
        url::form_urlencoded::parse(query.as_bytes()).into_owned().collect();
    let path = params.get("path").cloned().unwrap_or_else(|| "".to_string());

    ws::start(WsProcessor::new(path), &req, stream)
}
