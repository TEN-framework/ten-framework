//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::mpsc;

use super::TmanOutput;

// A TmanOutput implementation to send logs to the channel.
#[derive(Clone)]
pub struct TmanOutputChannel {
    pub sender: mpsc::Sender<String>,
}

impl TmanOutput for TmanOutputChannel {
    fn normal_line(&self, text: &str) {
        let _ = self.sender.send(format!("normal_line:{text}"));
    }

    fn normal_partial(&self, text: &str) {
        let _ = self.sender.send(format!("normal_partial:{text}"));
    }

    fn error_line(&self, text: &str) {
        let _ = self.sender.send(format!("error_line:{text}"));
    }

    fn error_partial(&self, text: &str) {
        let _ = self.sender.send(format!("error_partial:{text}"));
    }

    fn is_interactive(&self) -> bool {
        false
    }
}
