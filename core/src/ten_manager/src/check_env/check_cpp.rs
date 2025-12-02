//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;

use crate::output::TmanOutput;

/// Check C++ development environment (tgn, gcc/g++/clang toolchain).
/// Returns (tgn_installed, has_compiler).
pub fn check(out: Arc<Box<dyn TmanOutput>>) -> Result<(bool, bool)> {
    let mut tgn_installed = false;
    let mut has_compiler = false;

    // Check tgn
    let tgn_check = std::process::Command::new("tgn").arg("--help").output();

    match tgn_check {
        Ok(output) if output.status.success() => {
            // Find tgn path
            let which_output = std::process::Command::new("which").arg("tgn").output().ok();
            let path = if let Some(output) = which_output {
                String::from_utf8_lossy(&output.stdout).trim().to_string()
            } else {
                "unknown".to_string()
            };

            out.normal_line(&format!("✅ tgn installed ({})", path));
            tgn_installed = true;
        }
        _ => {
            out.normal_line("⚠️  tgn not installed");
            out.normal_line("   💡 To develop C++ extensions, please install tgn:");
            out.normal_line(
                "      curl -fsSL \
                 https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tgn/\
                 install_tgn.sh | bash",
            );
        }
    }

    // Check C++ compiler based on OS
    let os = std::env::consts::OS;

    if os == "linux" {
        // Check gcc/g++ on Linux
        let gcc_check = std::process::Command::new("gcc").arg("--version").output();

        match gcc_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                // Extract version (first line usually contains version info)
                if let Some(first_line) = version_str.lines().next() {
                    // Parse version number (e.g., "gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0")
                    if let Some(version) = first_line.split_whitespace().last() {
                        let which_output =
                            std::process::Command::new("which").arg("gcc").output().ok();
                        let path = if let Some(output) = which_output {
                            String::from_utf8_lossy(&output.stdout).trim().to_string()
                        } else {
                            "unknown".to_string()
                        };

                        out.normal_line(&format!("✅ gcc {} installed ({})", version, path));
                        has_compiler = true;
                    }
                }
            }
            _ => {
                out.normal_line("❌ gcc not found");
            }
        }

        let gpp_check = std::process::Command::new("g++").arg("--version").output();

        match gpp_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                if let Some(first_line) = version_str.lines().next() {
                    if let Some(version) = first_line.split_whitespace().last() {
                        let which_output =
                            std::process::Command::new("which").arg("g++").output().ok();
                        let path = if let Some(output) = which_output {
                            String::from_utf8_lossy(&output.stdout).trim().to_string()
                        } else {
                            "unknown".to_string()
                        };

                        out.normal_line(&format!("✅ g++ {} installed ({})", version, path));
                        has_compiler = true;
                    }
                }
            }
            _ => {
                out.normal_line("❌ g++ not found");
            }
        }

        if !has_compiler {
            out.normal_line("   💡 To install gcc/g++:");
            out.normal_line("      sudo apt-get install gcc g++");
        }
    } else if os == "macos" {
        // Check clang/clang++ on macOS
        let clang_check = std::process::Command::new("clang").arg("--version").output();

        match clang_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                // Extract version (format: "Apple clang version 14.0.0 ..." or "clang version
                // 15.0.0")
                if let Some(first_line) = version_str.lines().next() {
                    let version_info = if first_line.contains("Apple clang") {
                        first_line
                            .split_whitespace()
                            .nth(3)
                            .map(|v| format!("{} (Apple clang)", v))
                            .unwrap_or_else(|| "Apple clang".to_string())
                    } else {
                        first_line.split_whitespace().nth(2).unwrap_or("unknown").to_string()
                    };

                    let which_output =
                        std::process::Command::new("which").arg("clang").output().ok();
                    let path = if let Some(output) = which_output {
                        String::from_utf8_lossy(&output.stdout).trim().to_string()
                    } else {
                        "unknown".to_string()
                    };

                    out.normal_line(&format!("✅ clang {} installed ({})", version_info, path));
                    has_compiler = true;
                }
            }
            _ => {
                out.normal_line("❌ clang not found");
            }
        }

        let clangpp_check = std::process::Command::new("clang++").arg("--version").output();

        match clangpp_check {
            Ok(output) if output.status.success() => {
                let version_str = String::from_utf8_lossy(&output.stdout);
                if let Some(first_line) = version_str.lines().next() {
                    let version_info = if first_line.contains("Apple clang") {
                        first_line
                            .split_whitespace()
                            .nth(3)
                            .map(|v| format!("{} (Apple clang)", v))
                            .unwrap_or_else(|| "Apple clang".to_string())
                    } else {
                        first_line.split_whitespace().nth(2).unwrap_or("unknown").to_string()
                    };

                    out.normal_line(&format!("✅ clang++ {} installed", version_info));
                    has_compiler = true;
                }
            }
            _ => {
                out.normal_line("❌ clang++ not found");
            }
        }

        if !has_compiler {
            out.normal_line("   💡 To install Xcode Command Line Tools:");
            out.normal_line("      xcode-select --install");
        }
    }

    Ok((tgn_installed, has_compiler))
}
