//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use super::types::{CheckStatus, OsCheckResult, Suggestion};

/// Check operating system and architecture.
/// Returns structured result about OS support.
pub fn check() -> Result<OsCheckResult> {
    let os = std::env::consts::OS;
    let arch = std::env::consts::ARCH;

    // Map Rust's OS names to friendly names
    let os_name = match os {
        "linux" => "Linux",
        "macos" => "macOS",
        "windows" => "Windows",
        _ => os,
    };

    // Map Rust's arch names to common names
    let arch_name = match arch {
        "x86_64" => "x64",
        "aarch64" => "arm64",
        _ => arch,
    };

    // Check if the platform is supported
    let is_supported = matches!(
        (os, arch),
        ("linux", "x86_64") | ("linux", "aarch64") | ("macos", "x86_64") | ("macos", "aarch64")
    );

    let (status, suggestions) = if is_supported {
        (CheckStatus::Ok, vec![])
    } else if os == "windows" {
        (
            CheckStatus::Warning,
            vec![Suggestion {
                issue: "Windows support is under development".to_string(),
                command: None,
                help_text: Some("Coming soon".to_string()),
            }],
        )
    } else {
        (
            CheckStatus::Error,
            vec![Suggestion {
                issue: format!("Platform {} {} is not supported", os_name, arch_name),
                command: None,
                help_text: Some(
                    "Supported platforms: Linux x64/arm64, macOS x64/arm64".to_string(),
                ),
            }],
        )
    };

    Ok(OsCheckResult {
        os_name: os_name.to_string(),
        arch: arch_name.to_string(),
        is_supported,
        status,
        suggestions,
    })
}
