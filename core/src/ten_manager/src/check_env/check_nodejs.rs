//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;

use crate::output::TmanOutput;

/// Check Node.js development environment (node and npm commands).
/// Returns (has_nodejs, has_npm).
pub fn check(out: Arc<Box<dyn TmanOutput>>) -> Result<(bool, bool)> {
    let mut has_issues = false;
    let mut has_nodejs = false;
    let mut has_npm = false;

    // Check Node.js
    let node_check = std::process::Command::new("node").arg("--version").output();

    match node_check {
        Ok(output) if output.status.success() => {
            let version_str = String::from_utf8_lossy(&output.stdout);
            let version_str = version_str.trim();

            // Version format: "v22.0.0"
            let version_num = version_str.strip_prefix('v').unwrap_or(version_str);

            // Find node path
            let which_output = std::process::Command::new("which").arg("node").output().ok();
            let path = if let Some(output) = which_output {
                String::from_utf8_lossy(&output.stdout).trim().to_string()
            } else {
                "unknown".to_string()
            };

            out.normal_line(&format!("✅ Node.js {} installed ({})", version_str, path));

            has_nodejs = true;

            // Check if version is older than v16
            if let Some(major_str) = version_num.split('.').next() {
                if let Ok(major) = major_str.parse::<u32>() {
                    if major < 16 {
                        out.normal_line(&format!(
                            "   ⚠️  Node.js version is outdated ({}), may affect some features",
                            version_str
                        ));
                        out.normal_line("   💡 Recommend upgrading to Node.js 16 or higher");
                        has_issues = true;
                    } else if major < 18 {
                        out.normal_line("   💡 Consider upgrading to Node.js v18 LTS or higher");
                    }
                }
            }
        }
        _ => {
            out.normal_line("❌ Node.js not found");
            out.normal_line("   💡 Please install Node.js (v18 LTS or higher recommended)");
            out.normal_line("      https://nodejs.org/");
            has_issues = true;
        }
    }

    // Check npm
    let npm_check = std::process::Command::new("npm").arg("--version").output();

    match npm_check {
        Ok(output) if output.status.success() => {
            let version_str = String::from_utf8_lossy(&output.stdout);
            let version_str = version_str.trim();

            // Find npm path
            let which_output = std::process::Command::new("which").arg("npm").output().ok();
            let path = if let Some(output) = which_output {
                String::from_utf8_lossy(&output.stdout).trim().to_string()
            } else {
                "unknown".to_string()
            };

            out.normal_line(&format!("✅ npm {} installed ({})", version_str, path));
            has_npm = true;
        }
        _ => {
            out.normal_line("❌ npm not found");
            if !has_issues {
                out.normal_line(
                    "   ⚠️  npm should be installed with Node.js, please check installation",
                );
            }
        }
    }

    Ok((has_nodejs, has_npm))
}
