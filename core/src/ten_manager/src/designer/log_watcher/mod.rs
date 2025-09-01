//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix::{fut, Actor, AsyncContext, Handler, Message, StreamHandler};
use actix_web::{web, Error, HttpRequest, HttpResponse};
use actix_web_actors::ws;
use anyhow::Result;
use serde::{Deserialize, Serialize};
use serde_json::json;
use tokio::sync::Mutex;

use crate::{
    designer::DesignerState,
    fs::log_file_watcher::{LogFileContentStream, LogFileWatchOptions},
    log::LogLineInfo,
    pkg_info::property::get_log_file_path,
};

// Message types for WebSocket communication
#[derive(Message, Debug, Serialize, Deserialize)]
#[rtype(result = "()")]
pub struct StopWatching;

#[derive(Message, Debug, Serialize, Deserialize)]
#[rtype(result = "()")]
pub struct SetAppBaseDir {
    pub app_base_dir: String,
}

#[derive(Message, Debug, Serialize, Deserialize)]
#[rtype(result = "()")]
pub struct FileContent(pub LogLineInfo);

#[derive(Message, Debug, Serialize, Deserialize)]
#[rtype(result = "()")]
pub struct CloseConnection;

#[derive(Message, Debug, Serialize, Deserialize)]
#[rtype(result = "()")]
pub struct ErrorMessage(pub String);

#[derive(Message, Debug, Serialize, Deserialize)]
#[rtype(result = "()")]
pub struct InfoMessage(pub String);

#[derive(Message)]
#[rtype(result = "()")]
pub struct StoreWatcher(pub LogFileContentStream);

// WebSocket actor for log file watching.
struct WsLogWatcher {
    app_base_dir: Option<String>,
    file_watcher: Option<Arc<Mutex<LogFileContentStream>>>,
}

impl WsLogWatcher {
    fn new() -> Self {
        Self {
            app_base_dir: None,
            file_watcher: None,
        }
    }

    fn stop_watching(&mut self) {
        if let Some(watcher) = &self.file_watcher {
            // Create a new task to stop the watcher
            let watcher_clone = watcher.clone();
            tokio::spawn(async move {
                let mut guard = watcher_clone.lock().await;
                guard.stop();
            });
        }
        self.file_watcher = None;
    }
}

impl Actor for WsLogWatcher {
    type Context = ws::WebsocketContext<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        // Send a welcome message that we're ready to receive the app_base_dir.
        ctx.text(
            json!({
                "type": "ready",
                "message": "Ready to receive app_base_dir"
            })
            .to_string(),
        );
    }

    fn stopped(&mut self, _ctx: &mut Self::Context) {
        // Just set file_watcher to None to release our reference to it.
        // The tokio runtime will clean up any remaining tasks.
        self.file_watcher = None;
    }
}

impl Handler<SetAppBaseDir> for WsLogWatcher {
    type Result = ();

    fn handle(&mut self, msg: SetAppBaseDir, ctx: &mut Self::Context) -> Self::Result {
        // Store the app_base_dir.
        self.app_base_dir = Some(msg.app_base_dir.clone());

        // Clone what we need for the async task.
        let app_base_dir = msg.app_base_dir;
        let addr = ctx.address();

        // Spawn a task to handle the async file watching.
        ctx.spawn(fut::wrap_future(async move {
            // Get the log file path from property.json.
            let log_file_path = match get_log_file_path(&app_base_dir) {
                Some(path) => path,
                None => {
                    let _ = addr.try_send(ErrorMessage(
                        "No log file specified in property.json".to_string(),
                    ));
                    let _ = addr.try_send(CloseConnection);
                    return;
                }
            };

            // Create file watch options.
            let options = LogFileWatchOptions::default();

            // Start watching the file.
            match crate::fs::log_file_watcher::watch_log_file(&log_file_path, Some(options)).await {
                Ok(stream) => {
                    // Successfully started watching.
                    let _ = addr.try_send(InfoMessage("Started watching log file".to_string()));

                    // Send the stream to the actor to store.
                    let _ = addr.try_send(StoreWatcher(stream));
                }
                Err(e) => {
                    let _ = addr.try_send(ErrorMessage(e.to_string()));
                    let _ = addr.try_send(CloseConnection);
                }
            }
        }));
    }
}

impl Handler<FileContent> for WsLogWatcher {
    type Result = ();

    fn handle(&mut self, msg: FileContent, ctx: &mut Self::Context) -> Self::Result {
        // Send the entire LogLineInfo as JSON to the WebSocket client.
        match serde_json::to_string(&msg.0) {
            Ok(json_str) => {
                ctx.text(json_str);
            }
            Err(e) => {
                // Log the serialization error.
                eprintln!("Error serializing LogLineInfo to JSON: {e}");

                // Fallback to just the line content if serialization fails.
                ctx.text(msg.0.line);
            }
        }
    }
}

impl Handler<ErrorMessage> for WsLogWatcher {
    type Result = ();

    fn handle(&mut self, msg: ErrorMessage, ctx: &mut Self::Context) -> Self::Result {
        // Send the error message to the WebSocket client.
        ctx.text(
            json!({
                "type": "error",
                "message": msg.0
            })
            .to_string(),
        );
    }
}

impl Handler<InfoMessage> for WsLogWatcher {
    type Result = ();

    fn handle(&mut self, msg: InfoMessage, ctx: &mut Self::Context) -> Self::Result {
        // Send the info message to the WebSocket client.
        ctx.text(
            json!({
                "type": "info",
                "message": msg.0
            })
            .to_string(),
        );
    }
}

impl Handler<CloseConnection> for WsLogWatcher {
    type Result = ();

    fn handle(&mut self, _: CloseConnection, ctx: &mut Self::Context) -> Self::Result {
        // Stop watching and close the connection.
        self.stop_watching();
        ctx.close(None);
    }
}

impl Handler<StopWatching> for WsLogWatcher {
    type Result = ();

    fn handle(&mut self, _: StopWatching, ctx: &mut Self::Context) -> Self::Result {
        // Stop watching the file but keep the connection open.
        self.stop_watching();
        ctx.text(
            json!({
                "type": "info",
                "message": "Stopped watching log file"
            })
            .to_string(),
        );
    }
}

impl Handler<StoreWatcher> for WsLogWatcher {
    type Result = ();

    fn handle(&mut self, msg: StoreWatcher, ctx: &mut Self::Context) -> Self::Result {
        // Set up a task to read from the stream and send to WebSocket.
        let stream = Arc::new(Mutex::new(msg.0));
        self.file_watcher = Some(stream.clone());

        // Spawn a task to read from the stream and send to WebSocket.
        let addr = ctx.address();
        tokio::spawn(async move {
            loop {
                // Lock the mutex only for a short time to get the next item.
                let content = {
                    let mut stream_guard = stream.lock().await;
                    stream_guard.next().await
                };

                // Process the content outside the lock.
                match content {
                    Some(Ok(data)) => {
                        if addr.try_send(FileContent(data)).is_err() {
                            break;
                        }
                    }
                    Some(Err(e)) => {
                        let _ = addr.try_send(ErrorMessage(e.to_string()));
                        let _ = addr.try_send(CloseConnection);
                        break;
                    }
                    None => {
                        // If we exit the loop normally, the file watching has
                        // ended.
                        let _ = addr.try_send(CloseConnection);
                        break;
                    }
                }
            }
        });
    }
}

impl StreamHandler<Result<ws::Message, ws::ProtocolError>> for WsLogWatcher {
    fn handle(&mut self, msg: Result<ws::Message, ws::ProtocolError>, ctx: &mut Self::Context) {
        match msg {
            Ok(ws::Message::Ping(msg)) => ctx.pong(&msg),
            Ok(ws::Message::Text(text)) => {
                println!("Received text: {text}");

                // Try to parse the received message as JSON.
                if let Ok(json) = serde_json::from_str::<serde_json::Value>(&text) {
                    if let Some(message_type) = json.get("type").and_then(|v| v.as_str()) {
                        match message_type {
                            "stop" => {
                                let stop_msg = StopWatching;
                                <Self as Handler<StopWatching>>::handle(self, stop_msg, ctx);
                            }
                            "set_app_base_dir" => {
                                if let Some(app_base_dir) =
                                    json.get("app_base_dir").and_then(|v| v.as_str())
                                {
                                    // Check if we already have an app_base_dir.
                                    if self.app_base_dir.is_some() {
                                        ctx.text(
                                            json!({
                                                "type": "error",
                                                "message": "App base directory already set"
                                            })
                                            .to_string(),
                                        );
                                    } else {
                                        // Set the app base directory and start
                                        // watching.
                                        let set_app_base_dir_msg = SetAppBaseDir {
                                            app_base_dir: app_base_dir.to_string(),
                                        };
                                        <Self as Handler<SetAppBaseDir>>::handle(
                                            self,
                                            set_app_base_dir_msg,
                                            ctx,
                                        );
                                    }
                                } else {
                                    ctx.text(
                                        json!({
                                            "type": "error",
                                            "message": "Missing app_base_dir field"
                                        })
                                        .to_string(),
                                    );
                                }
                            }
                            _ => {
                                // Unknown message type.
                                ctx.text(
                                    json!({
                                        "type": "error",
                                        "message": "Unknown message type"
                                    })
                                    .to_string(),
                                );
                            }
                        }
                    }
                }
            }
            Ok(ws::Message::Binary(_)) => {
                // Binary messages are not expected.
                ctx.text(
                    json!({
                        "type": "error",
                        "message": "Binary messages are not supported"
                    })
                    .to_string(),
                );
            }
            _ => (),
        }
    }
}

// WebSocket endpoint handler
pub async fn log_watcher_endpoint(
    req: HttpRequest,
    stream: web::Payload,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<HttpResponse, Error> {
    // Start the WebSocket connection.
    ws::start(WsLogWatcher::new(), &req, stream)
}
