//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use super::types::{CheckStatus, CppCheckResult, Suggestion, ToolInfo};

/// Check whether Visual Studio (with any C++ workload) is installed by
/// querying vswhere.exe.
#[cfg(windows)]
fn is_vs_installed() -> bool {
    let vswhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe";
    if !std::path::Path::new(vswhere).exists() {
        return false;
    }

    std::process::Command::new(vswhere)
        .args([
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ])
        .output()
        .ok()
        .filter(|o| o.status.success())
        .map(|o| !String::from_utf8_lossy(&o.stdout).trim().is_empty())
        .unwrap_or(false)
}

#[cfg(not(windows))]
fn is_vs_installed() -> bool {
    false
}

/// Use vswhere.exe to locate clang-cl.exe from the latest VS installation.
/// clang-cl.exe is available when the "C++ Clang tools" component is installed.
#[cfg(windows)]
fn find_clang_cl_exe_via_vswhere() -> Option<String> {
    let vswhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe";
    if !std::path::Path::new(vswhere).exists() {
        return None;
    }

    let vs_path = std::process::Command::new(vswhere)
        .args([
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Llvm.Clang",
            "-property",
            "installationPath",
        ])
        .output()
        .ok()
        .filter(|o| o.status.success())
        .map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string())
        .filter(|s| !s.is_empty())?;

    let clang_cl =
        format!(r"{}\VC\Tools\Llvm\x64\bin\clang-cl.exe", vs_path);
    if std::path::Path::new(&clang_cl).exists() {
        Some(clang_cl)
    } else {
        None
    }
}

#[cfg(not(windows))]
fn find_clang_cl_exe_via_vswhere() -> Option<String> {
    None
}

/// Check C++ development environment (tgn, gcc/g++/clang toolchain).
/// Returns structured result about C++ tools.
pub fn check() -> Result<CppCheckResult> {
    let mut compilers = Vec::new();
    let mut tgn_installed = false;
    let mut has_compiler = false;
    let mut suggestions = Vec::new();

    // Check tgn
    // On Windows, tgn is typically a .bat file, so invoke through cmd.exe.
    let tgn_check = if cfg!(windows) {
        std::process::Command::new("cmd")
            .args(["/C", "tgn", "--help"])
            .output()
    } else {
        std::process::Command::new("tgn").arg("--help").output()
    };

    let tgn_info = match tgn_check {
        Ok(output) if output.status.success() => {
            // Find tgn path
            let path = if cfg!(windows) {
                std::process::Command::new("cmd")
                    .args(["/C", "where", "tgn"])
                    .output()
                    .ok()
                    .and_then(|output| {
                        if output.status.success() {
                            Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                        } else {
                            None
                        }
                    })
            } else {
                std::process::Command::new("which").arg("tgn").output().ok()
                    .and_then(|output| {
                        if output.status.success() {
                            Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                        } else {
                            None
                        }
                    })
            };

            tgn_installed = true;
            Some(ToolInfo {
                name: "tgn".to_string(),
                version: None,
                path,
                status: CheckStatus::Ok,
                notes: vec![],
            })
        }
        _ => {
            let tgn_install_cmd = if cfg!(windows) {
                "irm https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tgn/install_tgn.ps1 | iex".to_string()
            } else {
                "curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tgn/install_tgn.sh | bash".to_string()
            };
            suggestions.push(Suggestion {
                issue: "tgn not installed".to_string(),
                command: Some(tgn_install_cmd),
                help_text: Some("To develop C++ extensions, please install tgn".to_string()),
            });
            Some(ToolInfo {
                name: "tgn".to_string(),
                version: None,
                path: None,
                status: CheckStatus::Error,
                notes: vec!["Not installed".to_string()],
            })
        }
    };

    // Check C++ compiler based on OS
    let os = std::env::consts::OS;

    if os == "linux" {
        // Check gcc on Linux
        let gcc_check = std::process::Command::new("gcc").arg("--version").output();

        match gcc_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                // Extract version (first line usually contains version info)
                if let Some(first_line) = version_str.lines().next() {
                    // Parse version number (e.g., "gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0")
                    let version = first_line.split_whitespace().last().map(|s| s.to_string());

                    let which_output = std::process::Command::new("which").arg("gcc").output().ok();
                    let path = if let Some(output) = which_output {
                        Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                    } else {
                        None
                    };

                    compilers.push(ToolInfo {
                        name: "gcc".to_string(),
                        version,
                        path,
                        status: CheckStatus::Ok,
                        notes: vec![],
                    });
                    has_compiler = true;
                }
            }
            _ => {
                compilers.push(ToolInfo {
                    name: "gcc".to_string(),
                    version: None,
                    path: None,
                    status: CheckStatus::Error,
                    notes: vec!["Not found".to_string()],
                });
            }
        }

        // Check g++ on Linux
        let gpp_check = std::process::Command::new("g++").arg("--version").output();

        match gpp_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                if let Some(first_line) = version_str.lines().next() {
                    let version = first_line.split_whitespace().last().map(|s| s.to_string());

                    let which_output = std::process::Command::new("which").arg("g++").output().ok();
                    let path = if let Some(output) = which_output {
                        Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                    } else {
                        None
                    };

                    compilers.push(ToolInfo {
                        name: "g++".to_string(),
                        version,
                        path,
                        status: CheckStatus::Ok,
                        notes: vec![],
                    });
                    has_compiler = true;
                }
            }
            _ => {
                compilers.push(ToolInfo {
                    name: "g++".to_string(),
                    version: None,
                    path: None,
                    status: CheckStatus::Error,
                    notes: vec!["Not found".to_string()],
                });
            }
        }

        if !has_compiler {
            suggestions.push(Suggestion {
                issue: "gcc/g++ not found".to_string(),
                command: Some("sudo apt-get install gcc g++".to_string()),
                help_text: Some("To install gcc/g++".to_string()),
            });
        }
    } else if os == "macos" {
        // Check clang on macOS
        let clang_check = std::process::Command::new("clang").arg("--version").output();

        match clang_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                // Extract version (format: "Apple clang version 14.0.0 ..." or "clang version
                // 15.0.0")
                if let Some(first_line) = version_str.lines().next() {
                    let version = if first_line.contains("Apple clang") {
                        first_line.split_whitespace().nth(3).map(|v| format!("{} (Apple clang)", v))
                    } else {
                        first_line.split_whitespace().nth(2).map(|s| s.to_string())
                    };

                    let which_output =
                        std::process::Command::new("which").arg("clang").output().ok();
                    let path = if let Some(output) = which_output {
                        Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                    } else {
                        None
                    };

                    compilers.push(ToolInfo {
                        name: "clang".to_string(),
                        version,
                        path,
                        status: CheckStatus::Ok,
                        notes: vec![],
                    });
                    has_compiler = true;
                }
            }
            _ => {
                compilers.push(ToolInfo {
                    name: "clang".to_string(),
                    version: None,
                    path: None,
                    status: CheckStatus::Error,
                    notes: vec!["Not found".to_string()],
                });
            }
        }

        // Check clang++ on macOS
        let clangpp_check = std::process::Command::new("clang++").arg("--version").output();

        match clangpp_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                if let Some(first_line) = version_str.lines().next() {
                    let version = if first_line.contains("Apple clang") {
                        first_line.split_whitespace().nth(3).map(|v| format!("{} (Apple clang)", v))
                    } else {
                        first_line.split_whitespace().nth(2).map(|s| s.to_string())
                    };

                    let which_output =
                        std::process::Command::new("which").arg("clang++").output().ok();
                    let path = if let Some(output) = which_output {
                        Some(String::from_utf8_lossy(&output.stdout).trim().to_string())
                    } else {
                        None
                    };

                    compilers.push(ToolInfo {
                        name: "clang++".to_string(),
                        version,
                        path,
                        status: CheckStatus::Ok,
                        notes: vec![],
                    });
                    has_compiler = true;
                }
            }
            _ => {
                compilers.push(ToolInfo {
                    name: "clang++".to_string(),
                    version: None,
                    path: None,
                    status: CheckStatus::Error,
                    notes: vec!["Not found".to_string()],
                });
            }
        }

        if !has_compiler {
            suggestions.push(Suggestion {
                issue: "clang/clang++ not found".to_string(),
                command: Some("xcode-select --install".to_string()),
                help_text: Some("To install Xcode Command Line Tools".to_string()),
            });
        }
    } else if os == "windows" {
        // Check whether Visual Studio is installed via vswhere.
        let has_vs = is_vs_installed();

        // Check clang-cl.exe (shipped with VS "C++ Clang tools" component).
        let clang_cl_path = find_clang_cl_exe_via_vswhere();

        let clang_cl_check = if let Some(ref path) = clang_cl_path {
            std::process::Command::new(path).arg("--version").output()
        } else {
            // Fallback: maybe the user already has it in PATH.
            std::process::Command::new("clang-cl.exe")
                .arg("--version")
                .output()
        };

        match clang_cl_check {
            Ok(output) if output.status.success() => {
                let version_str =
                    String::from_utf8_lossy(&output.stdout);
                let version = version_str
                    .lines()
                    .next()
                    .map(|s| s.trim().to_string());

                let resolved_path = clang_cl_path.or_else(|| {
                    std::process::Command::new("where.exe")
                        .arg("clang-cl.exe")
                        .output()
                        .ok()
                        .and_then(|o| {
                            if o.status.success() {
                                Some(
                                    String::from_utf8_lossy(&o.stdout)
                                        .trim()
                                        .to_string(),
                                )
                            } else {
                                None
                            }
                        })
                });

                compilers.push(ToolInfo {
                    name: "clang-cl.exe".to_string(),
                    version,
                    path: resolved_path,
                    status: CheckStatus::Ok,
                    notes: vec![],
                });
                has_compiler = true;
            }
            _ => {
                compilers.push(ToolInfo {
                    name: "clang-cl.exe".to_string(),
                    version: None,
                    path: None,
                    status: CheckStatus::Error,
                    notes: vec!["Not found".to_string()],
                });
            }
        }

        if !has_compiler {
            if has_vs {
                // VS is installed but the Clang component is missing.
                suggestions.push(Suggestion {
                    issue: "clang-cl.exe not found".to_string(),
                    command: None,
                    help_text: Some(
                        "Visual Studio is installed but the C++ Clang tools \
                         component is missing. Please open Visual Studio \
                         Installer, click \"Modify\", and enable the \
                         \"C++ Clang tools\" component under \
                         \"Individual components\", then install it."
                            .to_string(),
                    ),
                });
            } else {
                // VS is not installed at all.
                suggestions.push(Suggestion {
                    issue: "clang-cl.exe not found".to_string(),
                    command: Some(
                        "Install Visual Studio with C++ build tools"
                            .to_string(),
                    ),
                    help_text: Some(
                        "Please install Visual Studio with C++ development \
                         tools. During installation, make sure to also enable \
                         the \"C++ Clang tools\" component under \
                         \"Individual components\"."
                            .to_string(),
                    ),
                });
            }
        }
    }

    // Determine overall status
    let status = if tgn_installed && has_compiler {
        CheckStatus::Ok
    } else if !tgn_installed && !has_compiler {
        CheckStatus::Error
    } else {
        CheckStatus::Warning
    };

    Ok(CppCheckResult {
        tgn: tgn_info,
        compilers,
        tgn_installed,
        has_compiler,
        status,
        suggestions,
    })
}
