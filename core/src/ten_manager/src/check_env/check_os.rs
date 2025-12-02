//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;

use crate::output::TmanOutput;

/// Check operating system and architecture.
/// Returns true if the OS is supported.
pub fn check(out: Arc<Box<dyn TmanOutput>>) -> Result<bool> {
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

    if is_supported {
        out.normal_line(&format!("✅ {} {} (Supported)", os_name, arch_name));
    } else if os == "windows" {
        out.normal_line(&format!("⚠️  {} {} (Not supported yet, coming soon)", os_name, arch_name));
        out.normal_line("   💡 Windows support is under development");
    } else {
        out.normal_line(&format!("❌ {} {} (Not supported)", os_name, arch_name));
        out.normal_line("   Supported platforms:");
        out.normal_line("     - Linux x64");
        out.normal_line("     - Linux arm64");
        out.normal_line("     - macOS x64 (Intel)");
        out.normal_line("     - macOS arm64 (Apple Silicon)");
    }

    Ok(is_supported)
}
