//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use super::types::{CheckStatus, GoCheckResult, Suggestion, ToolInfo};

/// Check Go development environment (go command, version >= 1.20).
/// Returns structured result about Go installation.
pub fn check() -> Result<GoCheckResult> {
    let mut go_info = None;
    let mut goroot = None;
    let mut gopath = None;
    let mut is_version_ok = false;
    let mut suggestions = Vec::new();

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
                                Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                            } else {
                                None
                            };

                            go_info = Some(ToolInfo {
                                name: "go".to_string(),
                                version: Some(version_part.to_string()),
                                path,
                                status: CheckStatus::Ok,
                                notes: vec![],
                            });

                            is_version_ok = true;

                            // Optionally get GOROOT
                            if let Ok(goroot_output) =
                                std::process::Command::new("go").arg("env").arg("GOROOT").output()
                            {
                                if goroot_output.status.success() {
                                    let root = String::from_utf8_lossy(&goroot_output.stdout)
                                        .trim()
                                        .to_string();
                                    if !root.is_empty() {
                                        goroot = Some(root);
                                    }
                                }
                            }

                            // Optionally get GOPATH
                            if let Ok(gopath_output) =
                                std::process::Command::new("go").arg("env").arg("GOPATH").output()
                            {
                                if gopath_output.status.success() {
                                    let gp = String::from_utf8_lossy(&gopath_output.stdout)
                                        .trim()
                                        .to_string();
                                    if !gp.is_empty() {
                                        gopath = Some(gp);
                                    }
                                }
                            }
                        } else {
                            let which_output =
                                std::process::Command::new("which").arg("go").output().ok();
                            let path = if let Some(output) = which_output {
                                Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                            } else {
                                None
                            };

                            go_info = Some(ToolInfo {
                                name: "go".to_string(),
                                version: Some(version_part.to_string()),
                                path,
                                status: CheckStatus::Warning,
                                notes: vec!["Version too old, requires >= 1.20".to_string()],
                            });

                            suggestions.push(Suggestion {
                                issue: format!("Go {} installed, but requires >= 1.20", version_part),
                                command: None,
                                help_text: Some("Please upgrade to Go 1.20 or higher from https://go.dev/dl/".to_string()),
                            });
                        }
                    }
                }
            }

            // If we can't parse the version, still report it
            if go_info.is_none() {
                go_info = Some(ToolInfo {
                    name: "go".to_string(),
                    version: Some(version_str.to_string()),
                    path: None,
                    status: CheckStatus::Warning,
                    notes: vec!["Unable to parse version".to_string()],
                });

                suggestions.push(Suggestion {
                    issue: "Unable to parse Go version".to_string(),
                    command: None,
                    help_text: Some("Please ensure Go >= 1.20 is installed".to_string()),
                });
            }
        }
        _ => {
            go_info = Some(ToolInfo {
                name: "go".to_string(),
                version: None,
                path: None,
                status: CheckStatus::Error,
                notes: vec!["Not found".to_string()],
            });

            suggestions.push(Suggestion {
                issue: "Go not found".to_string(),
                command: None,
                help_text: Some("Please install Go 1.20 or higher from https://go.dev/dl/".to_string()),
            });
        }
    }

    // Determine overall status
    let status = if is_version_ok {
        CheckStatus::Ok
    } else if go_info.as_ref().map(|g| g.version.is_some()).unwrap_or(false) {
        CheckStatus::Warning
    } else {
        CheckStatus::Error
    };

    Ok(GoCheckResult {
        go: go_info,
        goroot,
        gopath,
        is_version_ok,
        status,
        suggestions,
    })
}
