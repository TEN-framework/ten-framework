//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;

use crate::output::TmanOutput;

/// Check Go development environment (go command, version >= 1.20).
/// Returns true if Go >= 1.20 is installed.
pub fn check(out: Arc<Box<dyn TmanOutput>>) -> Result<bool> {
    // Check if go command exists
    let go_check = std::process::Command::new("go").arg("version").output();

    match go_check {
        Ok(output) if output.status.success() => {
            // Parse version from output
            let version_str = String::from_utf8_lossy(&output.stdout);
            let version_str = version_str.trim();

            // Extract version number (format: "go version go1.21.5 linux/amd64")
            if let Some(version_part) =
                version_str.split_whitespace().nth(2).and_then(|s| s.strip_prefix("go"))
            {
                // Parse major.minor version
                let version_parts: Vec<&str> = version_part.split('.').collect();
                if version_parts.len() >= 2 {
                    if let (Ok(major), Ok(minor)) =
                        (version_parts[0].parse::<u32>(), version_parts[1].parse::<u32>())
                    {
                        // Check if version >= 1.20
                        if major > 1 || (major == 1 && minor >= 20) {
                            // Find go path
                            let which_output =
                                std::process::Command::new("which").arg("go").output().ok();
                            let path = if let Some(output) = which_output {
                                String::from_utf8_lossy(&output.stdout).trim().to_string()
                            } else {
                                "unknown".to_string()
                            };

                            out.normal_line(&format!(
                                "✅ Go {} installed ({})",
                                version_part, path
                            ));

                            // Optionally show GOROOT and GOPATH
                            if let Ok(goroot_output) =
                                std::process::Command::new("go").arg("env").arg("GOROOT").output()
                            {
                                if goroot_output.status.success() {
                                    let goroot = String::from_utf8_lossy(&goroot_output.stdout)
                                        .trim()
                                        .to_string();
                                    if !goroot.is_empty() {
                                        out.normal_line(&format!("   GOROOT: {}", goroot));
                                    }
                                }
                            }

                            return Ok(true);
                        } else {
                            out.normal_line(&format!("⚠️  Go {} installed", version_part));
                            out.normal_line("   ❌ Go version too old, requires >= 1.20");
                            out.normal_line("   💡 Please upgrade to Go 1.20 or higher");
                            out.normal_line("      https://go.dev/dl/");
                            return Ok(false);
                        }
                    }
                }
            }

            // If we can't parse the version, still report it
            out.normal_line(&format!("⚠️  Go installed: {}", version_str));
            out.normal_line("   Unable to parse version, please ensure Go >= 1.20");
            Ok(false)
        }
        _ => {
            out.normal_line("❌ Go not found");
            out.normal_line("   💡 Please install Go 1.20 or higher");
            out.normal_line("      https://go.dev/dl/");
            Ok(false)
        }
    }
}
