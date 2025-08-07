//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{path::Path, process::Command, thread};

use actix::AsyncContext;
use actix_web_actors::ws::WebsocketContext;
use crossbeam_channel::{bounded, Sender};

use crate::designer::exec::RunCmdOutput;
use crate::log::{process_log_line, GraphResourcesLog, LogLineInfo};

use super::{msg::OutboundMsg, WsRunCmd};

// Add this struct to store shutdown senders.
pub struct ShutdownSenders {
    pub stdout: Sender<()>,
    pub stderr: Sender<()>,
    pub wait: Sender<()>,
}

// Output completion notification channels are created locally in cmd_run
// method.

impl WsRunCmd {
    pub fn cmd_run(
        &mut self,
        cmd: &String,
        ctx: &mut WebsocketContext<WsRunCmd>,
    ) {
        // Create shutdown channels for each thread.
        let (stdout_shutdown_tx, stdout_shutdown_rx) = bounded::<()>(1);
        let (stderr_shutdown_tx, stderr_shutdown_rx) = bounded::<()>(1);
        let (wait_shutdown_tx, wait_shutdown_rx) = bounded::<()>(1);

        // Create completion notification channels.
        let (stdout_done_tx, stdout_done_rx) = bounded::<()>(1);
        let (stderr_done_tx, stderr_done_rx) = bounded::<()>(1);

        // Store senders in the struct for later cleanup.
        self.shutdown_senders = Some(ShutdownSenders {
            stdout: stdout_shutdown_tx,
            stderr: stderr_shutdown_tx,
            wait: wait_shutdown_tx,
        });

        // Create command for different platforms
        let mut command;
        #[cfg(target_family = "windows")]
        {
            command = Command::new("cmd");
            command
                .arg("/C")
                .arg(cmd)
                // Set TEN_LOG_FORMATTER to json if any output is log content.
                .env(
                    "TEN_LOG_FORMATTER",
                    if self.stdout_is_log || self.stderr_is_log {
                        "json"
                    } else {
                        ""
                    },
                )
                .stdout(std::process::Stdio::piped())
                .stderr(std::process::Stdio::piped());
        }
        #[cfg(not(target_family = "windows"))]
        {
            command = Command::new("sh");
            command
                .arg("-c")
                .arg(format!("exec {cmd}"))
                .env(
                    "TEN_LOG_FORMATTER",
                    if self.stdout_is_log || self.stderr_is_log {
                        "json"
                    } else {
                        ""
                    },
                )
                .stdout(std::process::Stdio::piped())
                .stderr(std::process::Stdio::piped());
        }

        if let Some(ref dir) = self.working_directory {
            // Normalize path separators for cross-platform compatibility
            // On Windows, keep original path; on Unix-like systems, convert \
            // to /
            #[cfg(target_family = "windows")]
            let normalized_dir = dir;
            #[cfg(target_family = "unix")]
            let normalized_dir = dir.replace('\\', "/");

            let dir_path = Path::new(&normalized_dir);

            // Validate that the directory exists before setting it
            if dir_path.exists() && dir_path.is_dir() {
                command.current_dir(dir_path);
            } else {
                let err_msg = OutboundMsg::Error {
                    msg: format!(
                        "Working directory does not exist or is not a \
                         directory: {normalized_dir}"
                    ),
                };
                ctx.text(serde_json::to_string(&err_msg).unwrap());
                ctx.close(None);
                return;
            }
        }

        // Run the command.
        let child = match command.spawn() {
            Ok(c) => c,
            Err(e) => {
                let err_msg = OutboundMsg::Error {
                    msg: format!("Failed to spawn command: {e}"),
                };

                ctx.text(serde_json::to_string(&err_msg).unwrap());
                ctx.close(None);

                return;
            }
        };

        self.child = Some(child);

        // Spawn threads to read stdout & stderr.
        let stdout_child = self.child.as_mut().unwrap().stdout.take();
        let stderr_child = self.child.as_mut().unwrap().stderr.take();

        // Returns the address of the current actor. This address serves as a
        // reference that can be used in other threads or tasks to send messages
        // to the actor.
        let addr = ctx.address();

        // Track whether we have stdout/stderr to read.
        let has_stdout = stdout_child.is_some();
        let has_stderr = stderr_child.is_some();

        // Read stdout.
        if let Some(mut out) = stdout_child {
            let addr_stdout = addr.clone();
            let shutdown_rx = stdout_shutdown_rx;
            let done_tx = stdout_done_tx;
            let is_log = self.stdout_is_log;

            thread::spawn(move || {
                use std::io::{BufRead, BufReader};

                let reader = BufReader::new(&mut out);
                // Create a graph resources log instance for log processing.
                let mut graph_resources_log = GraphResourcesLog {
                    app_base_dir: String::new(),
                    app_uri: None,
                    graph_id: String::new(),
                    graph_name: None,
                    extension_threads: std::collections::HashMap::new(),
                };

                for line_res in reader.lines() {
                    // Check if we should terminate.
                    if shutdown_rx.try_recv().is_ok() {
                        break;
                    }

                    match line_res {
                        Ok(line) => {
                            if is_log {
                                // Process line as log content.
                                let metadata = process_log_line(
                                    &line,
                                    &mut graph_resources_log,
                                );
                                let log_line_info =
                                    LogLineInfo { line, metadata };
                                addr_stdout.do_send(RunCmdOutput::StdOutLog(
                                    log_line_info,
                                ));
                            } else {
                                // Process as normal stdout.
                                addr_stdout
                                    .do_send(RunCmdOutput::StdOutNormal(line));
                            }
                        }
                        Err(_) => break,
                    }
                }
                // Notify that stdout reading is finished.
                let _ = done_tx.send(());
            });
        } else {
            // If no stdout to read, immediately signal completion.
            let _ = stdout_done_tx.send(());
        }

        // Read stderr.
        if let Some(mut err) = stderr_child {
            let addr_stderr = addr.clone();
            let shutdown_rx = stderr_shutdown_rx;
            let done_tx = stderr_done_tx;
            let is_log = self.stderr_is_log;

            thread::spawn(move || {
                use std::io::{BufRead, BufReader};

                let reader = BufReader::new(&mut err);
                // Create a graph resources log instance for log processing.
                let mut graph_resources_log = GraphResourcesLog {
                    app_base_dir: String::new(),
                    app_uri: None,
                    graph_id: String::new(),
                    graph_name: None,
                    extension_threads: std::collections::HashMap::new(),
                };

                for line_res in reader.lines() {
                    // Check if we should terminate.
                    if shutdown_rx.try_recv().is_ok() {
                        break;
                    }

                    match line_res {
                        Ok(line) => {
                            if is_log {
                                // Process line as log content.
                                let metadata = process_log_line(
                                    &line,
                                    &mut graph_resources_log,
                                );
                                let log_line_info =
                                    LogLineInfo { line, metadata };
                                addr_stderr.do_send(RunCmdOutput::StdErrLog(
                                    log_line_info,
                                ));
                            } else {
                                // Process as normal stderr.
                                addr_stderr
                                    .do_send(RunCmdOutput::StdErrNormal(line));
                            }
                        }
                        Err(_) => break,
                    }
                }
                // Notify that stderr reading is finished.
                let _ = done_tx.send(());
            });
        } else {
            // If no stderr to read, immediately signal completion.
            let _ = stderr_done_tx.send(());
        }

        // Wait for child exit in another thread.
        let addr2 = ctx.address();
        if let Some(mut child) = self.child.take() {
            let shutdown_rx = wait_shutdown_rx;

            thread::spawn(move || {
                // First, wait for the process to exit
                let exit_code = loop {
                    let exit_status = crossbeam_channel::select! {
                        recv(shutdown_rx) -> _ => {
                            // Termination requested, kill the process.
                            let _ = child.kill();
                            match child.wait(){
                                Ok(status) => Some(status.code().unwrap_or(-1)),
                                Err(_) => Some(-1),
                            }
                        },
                        default => {
                            // Non-blocking check for process exit
                            match child.try_wait() {
                                Ok(Some(status)) => Some(status.code().unwrap_or(-1)),
                                Ok(None) => {
                                    // Process still running, continue waiting
                                    None
                                },
                                Err(_) => Some(-1),
                            }
                        }
                    };

                    if let Some(code) = exit_status {
                        break code;
                    }

                    // If no exit code (process still running),
                    // continue the loop
                    std::thread::sleep(std::time::Duration::from_millis(50));
                };

                // Process has exited, now wait for all output threads to
                // complete

                // Wait for stdout completion if it exists
                if has_stdout {
                    let _ = stdout_done_rx.recv();
                }

                // Wait for stderr completion if it exists
                if has_stderr {
                    let _ = stderr_done_rx.recv();
                }

                // All output has been processed, now send exit
                addr2.do_send(RunCmdOutput::Exit(exit_code));
            });
        }
    }

    // Call this when the actor is stopping or websocket is closing.
    pub fn cleanup_threads(&mut self) {
        // Signal all threads to terminate.
        if let Some(senders) = self.shutdown_senders.take() {
            let _ = senders.stdout.send(());
            let _ = senders.stderr.send(());
            let _ = senders.wait.send(());
        }

        // Force kill child process if it exists.
        if let Some(mut child) = self.child.take() {
            let _ = child.kill();
        }
    }
}
