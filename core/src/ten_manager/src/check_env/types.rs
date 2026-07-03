//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use serde::{Deserialize, Serialize};

/// Status of an individual check item.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum CheckStatus {
    /// Check passed successfully.
    Ok,
    /// Check passed with warnings.
    Warning,
    /// Check failed.
    Error,
    /// Check was not performed or not applicable.
    NotApplicable,
}

/// Information about a tool or component.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolInfo {
    /// Tool name (e.g., "gcc", "python3", "node").
    pub name: String,
    /// Version string (e.g., "11.4.0", "3.10.12").
    pub version: Option<String>,
    /// Installation path.
    pub path: Option<String>,
    /// Check status.
    pub status: CheckStatus,
    /// Additional notes or warnings.
    pub notes: Vec<String>,
}

/// Suggestion for resolving an issue.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Suggestion {
    /// Description of the issue.
    pub issue: String,
    /// Suggested command or action.
    pub command: Option<String>,
    /// Additional help text or URL.
    pub help_text: Option<String>,
}

/// Result of checking the operating system.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OsCheckResult {
    /// OS name (e.g., "Linux", "macOS", "Windows").
    pub os_name: String,
    /// Architecture (e.g., "x64", "arm64").
    pub arch: String,
    /// Whether the platform is supported.
    pub is_supported: bool,
    /// Overall status.
    pub status: CheckStatus,
    /// Suggestions if not supported.
    pub suggestions: Vec<Suggestion>,
}

/// Result of checking C++ development environment.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CppCheckResult {
    /// TGN tool information.
    pub tgn: Option<ToolInfo>,
    /// Compiler information (gcc/g++/clang/clang++).
    pub compilers: Vec<ToolInfo>,
    /// Whether TGN is installed.
    pub tgn_installed: bool,
    /// Whether any compiler is available.
    pub has_compiler: bool,
    /// Overall status.
    pub status: CheckStatus,
    /// Suggestions for missing tools.
    pub suggestions: Vec<Suggestion>,
}

/// Result of checking Python environment.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PythonCheckResult {
    /// Python tool information.
    pub python: Option<ToolInfo>,
    /// Pip tool information.
    pub pip: Option<ToolInfo>,
    /// Whether Python 3.10 is installed.
    pub is_correct_version: bool,
    /// Overall status.
    pub status: CheckStatus,
    /// Suggestions for issues.
    pub suggestions: Vec<Suggestion>,
}

/// Result of checking Go environment.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GoCheckResult {
    /// Go tool information.
    pub go: Option<ToolInfo>,
    /// GOROOT path.
    pub goroot: Option<String>,
    /// GOPATH path.
    pub gopath: Option<String>,
    /// Whether Go >= 1.20 is installed.
    pub is_version_ok: bool,
    /// Overall status.
    pub status: CheckStatus,
    /// Suggestions for issues.
    pub suggestions: Vec<Suggestion>,
}

/// Result of checking Node.js environment.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeJsCheckResult {
    /// Node.js tool information.
    pub node: Option<ToolInfo>,
    /// npm tool information.
    pub npm: Option<ToolInfo>,
    /// Whether Node.js is installed.
    pub has_nodejs: bool,
    /// Whether npm is installed.
    pub has_npm: bool,
    /// Overall status.
    pub status: CheckStatus,
    /// Suggestions for issues.
    pub suggestions: Vec<Suggestion>,
}

/// Combined result of all environment checks.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnvCheckResult {
    /// Operating system check result.
    pub os: OsCheckResult,
    /// C++ environment check result.
    pub cpp: CppCheckResult,
    /// Python environment check result.
    pub python: PythonCheckResult,
    /// Go environment check result.
    pub go: GoCheckResult,
    /// Node.js environment check result.
    pub nodejs: NodeJsCheckResult,
}
