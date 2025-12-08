//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use super::types::{
    CheckStatus, CppCheckResult, EnvCheckResult, GoCheckResult, NodeJsCheckResult,
    OsCheckResult, PythonCheckResult, Suggestion, ToolInfo,
};
use crate::output::TmanOutput;

/// Format check results for output.
pub trait CheckResultFormatter {
    /// Format the entire environment check result.
    fn format_env_check(&self, result: &EnvCheckResult, out: Arc<Box<dyn TmanOutput>>);

    /// Format OS check result.
    fn format_os_check(&self, result: &OsCheckResult, out: Arc<Box<dyn TmanOutput>>);

    /// Format C++ check result.
    fn format_cpp_check(&self, result: &CppCheckResult, out: Arc<Box<dyn TmanOutput>>);

    /// Format Python check result.
    fn format_python_check(&self, result: &PythonCheckResult, out: Arc<Box<dyn TmanOutput>>);

    /// Format Go check result.
    fn format_go_check(&self, result: &GoCheckResult, out: Arc<Box<dyn TmanOutput>>);

    /// Format Node.js check result.
    fn format_nodejs_check(&self, result: &NodeJsCheckResult, out: Arc<Box<dyn TmanOutput>>);
}

/// CLI formatter with emoji and colored output.
pub struct CliFormatter;

impl CliFormatter {
    fn format_status_icon(&self, status: &CheckStatus) -> &'static str {
        match status {
            CheckStatus::Ok => "✅",
            CheckStatus::Warning => "⚠️ ",
            CheckStatus::Error => "❌",
            CheckStatus::NotApplicable => "➖",
        }
    }

    fn format_tool_info(&self, tool: &ToolInfo, out: Arc<Box<dyn TmanOutput>>) {
        let icon = self.format_status_icon(&tool.status);
        let version_str = tool.version.as_deref().unwrap_or("unknown");
        let path_str = tool
            .path
            .as_ref()
            .map(|p| format!(" ({})", p))
            .unwrap_or_default();

        match tool.status {
            CheckStatus::Ok => {
                out.normal_line(&format!("{} {} {} installed{}", icon, tool.name, version_str, path_str));
            }
            CheckStatus::Warning => {
                out.normal_line(&format!("{} {} {}{}", icon, tool.name, version_str, path_str));
            }
            CheckStatus::Error => {
                out.normal_line(&format!("{} {} {}", icon, tool.name, tool.notes.first().unwrap_or(&"error".to_string())));
            }
            CheckStatus::NotApplicable => {}
        }

        // Print notes
        for note in &tool.notes {
            if tool.status != CheckStatus::Error {
                out.normal_line(&format!("   {}", note));
            }
        }
    }

    fn format_suggestions(&self, suggestions: &[Suggestion], out: Arc<Box<dyn TmanOutput>>) {
        for suggestion in suggestions {
            out.normal_line(&format!("   💡 {}", suggestion.issue));
            if let Some(ref help) = suggestion.help_text {
                out.normal_line(&format!("      {}", help));
            }
            if let Some(ref cmd) = suggestion.command {
                out.normal_line(&format!("      {}", cmd));
            }
        }
    }
}

impl CheckResultFormatter for CliFormatter {
    fn format_env_check(&self, result: &EnvCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        self.format_os_check(&result.os, out.clone());
        out.normal_line("");
        self.format_cpp_check(&result.cpp, out.clone());
        out.normal_line("");
        self.format_python_check(&result.python, out.clone());
        out.normal_line("");
        self.format_go_check(&result.go, out.clone());
        out.normal_line("");
        self.format_nodejs_check(&result.nodejs, out);
    }

    fn format_os_check(&self, result: &OsCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        let icon = self.format_status_icon(&result.status);
        let support_text = if result.is_supported {
            "(Supported)"
        } else if result.os_name == "Windows" {
            "(Not supported yet, coming soon)"
        } else {
            "(Not supported)"
        };

        out.normal_line(&format!("{} {} {} {}", icon, result.os_name, result.arch, support_text));

        if !result.suggestions.is_empty() {
            self.format_suggestions(&result.suggestions, out.clone());
            if !result.is_supported && result.os_name != "Windows" {
                out.normal_line("   Supported platforms:");
                out.normal_line("     - Linux x64");
                out.normal_line("     - Linux arm64");
                out.normal_line("     - macOS x64 (Intel)");
                out.normal_line("     - macOS arm64 (Apple Silicon)");
            }
        }
    }

    fn format_cpp_check(&self, result: &CppCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        if let Some(ref tgn) = result.tgn {
            self.format_tool_info(tgn, out.clone());
        }

        for compiler in &result.compilers {
            self.format_tool_info(compiler, out.clone());
        }

        if !result.suggestions.is_empty() {
            self.format_suggestions(&result.suggestions, out);
        }
    }

    fn format_python_check(&self, result: &PythonCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        if let Some(ref python) = result.python {
            self.format_tool_info(python, out.clone());
        }

        if let Some(ref pip) = result.pip {
            self.format_tool_info(pip, out.clone());
        }

        if !result.suggestions.is_empty() {
            self.format_suggestions(&result.suggestions, out);
        }
    }

    fn format_go_check(&self, result: &GoCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        if let Some(ref go) = result.go {
            self.format_tool_info(go, out.clone());

            // Show GOROOT if available
            if result.is_version_ok {
                if let Some(ref goroot) = result.goroot {
                    out.normal_line(&format!("   GOROOT: {}", goroot));
                }
            }
        }

        if !result.suggestions.is_empty() {
            self.format_suggestions(&result.suggestions, out);
        }
    }

    fn format_nodejs_check(&self, result: &NodeJsCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        if let Some(ref node) = result.node {
            self.format_tool_info(node, out.clone());
        }

        if let Some(ref npm) = result.npm {
            self.format_tool_info(npm, out.clone());
        }

        if !result.suggestions.is_empty() {
            self.format_suggestions(&result.suggestions, out);
        }
    }
}

/// JSON formatter for structured output (WebSocket, HTTP API, etc.).
pub struct JsonFormatter;

impl JsonFormatter {
    /// Serialize the environment check result to JSON string.
    pub fn to_json(result: &EnvCheckResult) -> Result<String, serde_json::Error> {
        serde_json::to_string_pretty(result)
    }
}

impl CheckResultFormatter for JsonFormatter {
    fn format_env_check(&self, result: &EnvCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        match Self::to_json(result) {
            Ok(json) => out.normal_line(&json),
            Err(e) => out.error_line(&format!("Failed to serialize to JSON: {}", e)),
        }
    }

    fn format_os_check(&self, result: &OsCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        match serde_json::to_string_pretty(result) {
            Ok(json) => out.normal_line(&json),
            Err(e) => out.error_line(&format!("Failed to serialize to JSON: {}", e)),
        }
    }

    fn format_cpp_check(&self, result: &CppCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        match serde_json::to_string_pretty(result) {
            Ok(json) => out.normal_line(&json),
            Err(e) => out.error_line(&format!("Failed to serialize to JSON: {}", e)),
        }
    }

    fn format_python_check(&self, result: &PythonCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        match serde_json::to_string_pretty(result) {
            Ok(json) => out.normal_line(&json),
            Err(e) => out.error_line(&format!("Failed to serialize to JSON: {}", e)),
        }
    }

    fn format_go_check(&self, result: &GoCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        match serde_json::to_string_pretty(result) {
            Ok(json) => out.normal_line(&json),
            Err(e) => out.error_line(&format!("Failed to serialize to JSON: {}", e)),
        }
    }

    fn format_nodejs_check(&self, result: &NodeJsCheckResult, out: Arc<Box<dyn TmanOutput>>) {
        match serde_json::to_string_pretty(result) {
            Ok(json) => out.normal_line(&json),
            Err(e) => out.error_line(&format!("Failed to serialize to JSON: {}", e)),
        }
    }
}
