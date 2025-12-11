//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use super::types::{CheckStatus, NodeJsCheckResult, Suggestion, ToolInfo};

/// Check Node.js development environment (node and npm commands).
/// Returns structured result about Node.js and npm installation.
pub fn check() -> Result<NodeJsCheckResult> {
    let mut has_nodejs = false;
    let mut has_npm = false;
    let mut suggestions = Vec::new();

    // Check Node.js
    let node_check = std::process::Command::new("node").arg("--version").output();

    let node_info = match node_check {
        Ok(output) if output.status.success() => {
            let version_str = String::from_utf8_lossy(&output.stdout);
            let version_str = version_str.trim();

            // Version format: "v22.0.0"
            let version_num = version_str.strip_prefix('v').unwrap_or(version_str);

            // Find node path
            let which_output = std::process::Command::new("which").arg("node").output().ok();
            let path = if let Some(output) = which_output {
                Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
            } else {
                None
            };

            has_nodejs = true;

            // Check version and set status
            let mut notes = vec![];
            let status = if let Some(major_str) = version_num.split('.').next() {
                if let Ok(major) = major_str.parse::<u32>() {
                    if major < 16 {
                        notes.push("Version is outdated, may affect some features".to_string());
                        suggestions.push(Suggestion {
                            issue: format!("Node.js version {} is outdated", version_str),
                            command: None,
                            help_text: Some("Recommend upgrading to Node.js 16 or higher".to_string()),
                        });
                        CheckStatus::Warning
                    } else if major < 18 {
                        notes.push("Consider upgrading to Node.js v18 LTS or higher".to_string());
                        CheckStatus::Warning
                    } else {
                        CheckStatus::Ok
                    }
                } else {
                    CheckStatus::Ok
                }
            } else {
                CheckStatus::Ok
            };

            Some(ToolInfo {
                name: "node".to_string(),
                version: Some(version_str.to_string()),
                path,
                status,
                notes,
            })
        }
        _ => {
            suggestions.push(Suggestion {
                issue: "Node.js not found".to_string(),
                command: None,
                help_text: Some("Please install Node.js (v18 LTS or higher recommended) from https://nodejs.org/".to_string()),
            });

            Some(ToolInfo {
                name: "node".to_string(),
                version: None,
                path: None,
                status: CheckStatus::Error,
                notes: vec!["Not found".to_string()],
            })
        }
    };

    // Check npm
    let npm_check = std::process::Command::new("npm").arg("--version").output();

    let npm_info = match npm_check {
        Ok(output) if output.status.success() => {
            let version_str = String::from_utf8_lossy(&output.stdout);
            let version_str = version_str.trim();

            // Find npm path
            let which_output = std::process::Command::new("which").arg("npm").output().ok();
            let path = if let Some(output) = which_output {
                Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
            } else {
                None
            };

            has_npm = true;
            Some(ToolInfo {
                name: "npm".to_string(),
                version: Some(version_str.to_string()),
                path,
                status: CheckStatus::Ok,
                notes: vec![],
            })
        }
        _ => {
            if has_nodejs {
                suggestions.push(Suggestion {
                    issue: "npm not found".to_string(),
                    command: None,
                    help_text: Some("npm should be installed with Node.js, please check installation".to_string()),
                });
            }

            Some(ToolInfo {
                name: "npm".to_string(),
                version: None,
                path: None,
                status: CheckStatus::Error,
                notes: vec!["Not found".to_string()],
            })
        }
    };

    // Determine overall status
    let status = if has_nodejs && has_npm {
        // Check if node has warnings
        if let Some(ref node) = node_info {
            if node.status == CheckStatus::Warning {
                CheckStatus::Warning
            } else {
                CheckStatus::Ok
            }
        } else {
            CheckStatus::Ok
        }
    } else if has_nodejs || has_npm {
        CheckStatus::Warning
    } else {
        CheckStatus::Error
    };

    Ok(NodeJsCheckResult {
        node: node_info,
        npm: npm_info,
        has_nodejs,
        has_npm,
        status,
        suggestions,
    })
}
