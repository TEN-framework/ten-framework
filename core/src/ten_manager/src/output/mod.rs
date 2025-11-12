//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod channel;
pub mod cli;

/// Abstract all log output methods: CLI, WebSocket, etc.
pub trait TmanOutput: Send + Sync {
    /// General information.
    fn normal_line(&self, text: &str);
    fn normal_partial(&self, text: &str);

    /// Error information.
    fn error_line(&self, text: &str);
    fn error_partial(&self, text: &str);

    /// Whether it is interactive (e.g., can block waiting for user input in CLI
    /// environment).
    fn is_interactive(&self) -> bool;
}
