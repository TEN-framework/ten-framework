//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;

use crate::output::TmanOutput;

/// Check Python development environment (python3 command, version == 3.10).
/// Returns true if Python 3.10 is installed.
pub fn check(out: Arc<Box<dyn TmanOutput>>) -> Result<bool> {
    // Check if python3 command exists
    let python_check = std::process::Command::new("python3").arg("--version").output();

    match python_check {
        Ok(output) if output.status.success() => {
            // Parse version from output
            let version_str = String::from_utf8_lossy(&output.stdout);
            let version_str = version_str.trim();

            // Extract version number (format: "Python 3.10.12")
            if let Some(version_part) = version_str.strip_prefix("Python ") {
                // Parse major.minor version
                let version_parts: Vec<&str> = version_part.split('.').collect();
                if version_parts.len() >= 2 {
                    if let (Ok(major), Ok(minor)) =
                        (version_parts[0].parse::<u32>(), version_parts[1].parse::<u32>())
                    {
                        // Check if version == 3.10
                        if major == 3 && minor == 10 {
                            // Find python3 path
                            let which_output =
                                std::process::Command::new("which").arg("python3").output().ok();
                            let path = if let Some(output) = which_output {
                                String::from_utf8_lossy(&output.stdout).trim().to_string()
                            } else {
                                "unknown".to_string()
                            };

                            out.normal_line(&format!(
                                "✅ Python {} installed ({})",
                                version_part, path
                            ));

                            // Check pip3
                            let pip_check =
                                std::process::Command::new("pip3").arg("--version").output();
                            if let Ok(pip_output) = pip_check {
                                if pip_output.status.success() {
                                    let pip_version = String::from_utf8_lossy(&pip_output.stdout);
                                    if let Some(version_info) =
                                        pip_version.split_whitespace().nth(1)
                                    {
                                        out.normal_line(&format!(
                                            "✅ pip3 {} installed",
                                            version_info
                                        ));
                                    }
                                }
                            }

                            return Ok(true);
                        } else {
                            out.normal_line(&format!("⚠️  Python {} installed", version_part));
                            out.normal_line("   ❌ TEN Framework only supports Python 3.10");
                            out.normal_line("   💡 Please use pyenv to install Python 3.10:");
                            out.normal_line("      pyenv install 3.10.18");
                            out.normal_line("      pyenv local 3.10.18");
                            return Ok(false);
                        }
                    }
                }
            }

            // If we can't parse the version, still report it
            out.normal_line(&format!("⚠️  Python installed: {}", version_str));
            out.normal_line("   Unable to parse version, please ensure Python 3.10 is installed");
            Ok(false)
        }
        _ => {
            out.normal_line("❌ Python not found");
            out.normal_line("   💡 Please install Python 3.10");
            out.normal_line("      Using pyenv (recommended):");
            out.normal_line("      pyenv install 3.10.18");
            out.normal_line("      pyenv local 3.10.18");
            Ok(false)
        }
    }
}
