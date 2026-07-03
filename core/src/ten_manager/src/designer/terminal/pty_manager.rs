//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    io::{BufRead, BufReader, Write},
    path::{Path, PathBuf},
    thread::{self, sleep},
    time::Duration,
};

use portable_pty::{native_pty_system, CommandBuilder, PtyPair, PtySize};

use super::PtyMessage;

pub struct PtyManager {
    pty_pair: PtyPair,
    writer: Box<dyn Write + Send>,
    child: Option<Box<dyn portable_pty::Child + Send>>,
}

impl PtyManager {
    pub fn new(path: String) -> Self {
        // Validate the path.
        let sanitized_path = if path.is_empty() {
            std::env::current_dir().unwrap_or_else(|_| PathBuf::from("/"))
        } else {
            let path = Path::new(&path);
            if path.is_absolute() {
                path.to_path_buf()
            } else {
                std::env::current_dir().unwrap_or_else(|_| PathBuf::from("/")).join(path)
            }
        };

        let pty_system = native_pty_system();

        let pty_pair = pty_system
            .openpty(PtySize {
                rows: 24,
                cols: 80,
                pixel_width: 0,
                pixel_height: 0,
            })
            .unwrap();

        #[cfg(target_os = "windows")]
        let mut cmd_builder = CommandBuilder::new("powershell.exe");
        #[cfg(not(target_os = "windows"))]
        let mut cmd_builder = CommandBuilder::new("bash");

        // Add the $TERM env variable so we can use clear and other commands.
        #[cfg(target_os = "windows")]
        cmd_builder.env("TERM", "cygwin");
        #[cfg(not(target_os = "windows"))]
        cmd_builder.env("TERM", "xterm-256color");

        cmd_builder.cwd(sanitized_path.clone());

        // Create the shell process.
        let child = pty_pair.slave.spawn_command(cmd_builder).unwrap();

        let writer = pty_pair.master.take_writer().unwrap();

        Self {
            pty_pair,
            writer,
            child: Some(child),
        }
    }

    pub fn start(&mut self) -> std::sync::mpsc::Receiver<PtyMessage> {
        let (tx, rx) = std::sync::mpsc::channel::<PtyMessage>();

        // Create a thread to wait for the process to finish.
        let reader = self.pty_pair.master.try_clone_reader().unwrap();
        if let Some(mut child) = self.child.take() {
            let tx_clone = tx.clone();
            std::thread::spawn(move || {
                let status = child.wait().unwrap();
                println!("The terminal process exited: {}", status.exit_code() as i32);

                // Send Exit message after the child process exited.
                let _ = tx_clone.send(PtyMessage::Exit(status.exit_code() as i32));
            });
        }

        // Create a thread to interact with the terminal process.
        thread::spawn(move || {
            let mut buf = BufReader::new(reader);

            loop {
                sleep(Duration::from_millis(10));

                // Read all available text.
                let data = buf.fill_buf().unwrap().to_vec();
                buf.consume(data.len());

                // Send te data to the websocket if necessary.
                if !data.is_empty() {
                    if let Err(e) = tx.send(PtyMessage::Buffer(data)) {
                        println!("Failed to send pty message: {e}");
                        break;
                    }
                }
            }
        });

        rx
    }

    pub fn write_to_pty(&mut self, data: &str) -> Result<(), ()> {
        write!(self.writer, "{data}").map_err(|_| ())
    }

    pub fn resize_pty(&self, cols: u16, rows: u16) -> Result<(), ()> {
        self.pty_pair
            .master
            .resize(PtySize {
                rows,
                cols,
                ..Default::default()
            })
            .map_err(|_| ())
    }
}
