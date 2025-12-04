//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use super::types::{CheckStatus, PythonCheckResult, Suggestion, ToolInfo};

/// Check Python development environment (python3 command, version == 3.10).
/// Returns structured result about Python installation.
pub fn check() -> Result<PythonCheckResult> {
    let mut python_info = None;
    let mut pip_info = None;
    let mut is_correct_version = false;
    let mut suggestions = Vec::new();

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
                                Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                            } else {
                                None
                            };

                            python_info = Some(ToolInfo {
                                name: "python3".to_string(),
                                version: Some(version_part.to_string()),
                                path,
                                status: CheckStatus::Ok,
                                notes: vec![],
                            });

                            is_correct_version = true;

                            // Check pip3
                            let pip_check =
                                std::process::Command::new("pip3").arg("--version").output();
                            if let Ok(pip_output) = pip_check {
                                if pip_output.status.success() {
                                    let pip_version = String::from_utf8_lossy(&pip_output.stdout);
                                    let version_info =
                                        pip_version.split_whitespace().nth(1).map(|s| s.to_string());

                                    pip_info = Some(ToolInfo {
                                        name: "pip3".to_string(),
                                        version: version_info,
                                        path: None,
                                        status: CheckStatus::Ok,
                                        notes: vec![],
                                    });
                                }
                            }
                        } else {
                            let which_output =
                                std::process::Command::new("which").arg("python3").output().ok();
                            let path = if let Some(output) = which_output {
                                Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                            } else {
                                None
                            };

                            python_info = Some(ToolInfo {
                                name: "python3".to_string(),
                                version: Some(version_part.to_string()),
                                path,
                                status: CheckStatus::Warning,
                                notes: vec!["TEN Framework only supports Python 3.10".to_string()],
                            });

                            suggestions.push(Suggestion {
                                issue: format!("Python {} installed, but TEN Framework only supports Python 3.10", version_part),
                                command: Some("pyenv install 3.10.18 && pyenv local 3.10.18".to_string()),
                                help_text: Some("Please use pyenv to install Python 3.10".to_string()),
                            });
                        }
                    }
                }
            }

            // If we can't parse the version, still report it
            if python_info.is_none() {
                python_info = Some(ToolInfo {
                    name: "python3".to_string(),
                    version: Some(version_str.to_string()),
                    path: None,
                    status: CheckStatus::Warning,
                    notes: vec!["Unable to parse version".to_string()],
                });

                suggestions.push(Suggestion {
                    issue: "Unable to parse Python version".to_string(),
                    command: None,
                    help_text: Some("Please ensure Python 3.10 is installed".to_string()),
                });
            }
        }
        _ => {
            python_info = Some(ToolInfo {
                name: "python3".to_string(),
                version: None,
                path: None,
                status: CheckStatus::Error,
                notes: vec!["Not found".to_string()],
            });

            suggestions.push(Suggestion {
                issue: "Python not found".to_string(),
                command: Some("pyenv install 3.10.18 && pyenv local 3.10.18".to_string()),
                help_text: Some("Please install Python 3.10 using pyenv (recommended)".to_string()),
            });
        }
    }

    // Determine overall status
    let status = if is_correct_version {
        CheckStatus::Ok
    } else if python_info.as_ref().map(|p| p.version.is_some()).unwrap_or(false) {
        CheckStatus::Warning
    } else {
        CheckStatus::Error
    };

    Ok(PythonCheckResult {
        python: python_info,
        pip: pip_info,
        is_correct_version,
        status,
        suggestions,
    })
}
