//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;
use clap::{ArgMatches, Command};

use crate::{
    check_env::{
        check_cpp, check_go, check_nodejs, check_os, check_python,
        formatter::{CheckResultFormatter, CliFormatter},
        types::{CheckStatus, EnvCheckResult},
    },
    designer::storage::in_memory::TmanStorageInMemory,
    home::config::TmanConfig,
    output::TmanOutput,
};

#[derive(Debug)]
pub struct CheckEnvCommand {}

pub fn create_sub_cmd(_args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("env").about("Check development environment for TEN Framework").after_help(
        "Check if your system has the required development environments:\n\n  - Operating System \
         (Linux/macOS x64/arm64)\n  - Python 3.10\n  - Go 1.20+\n  - Node.js and npm\n  - C++ \
         toolchain (tgn, gcc/clang)",
    )
}

pub fn parse_sub_cmd(_sub_cmd_args: &ArgMatches) -> Result<CheckEnvCommand> {
    Ok(CheckEnvCommand {})
}

pub async fn execute_cmd(
    _tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    _cmd: CheckEnvCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    out.normal_line("🔍 Checking TEN Framework development environment...");
    out.normal_line("");
    out.normal_line("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    out.normal_line("");

    // Perform all checks and collect structured results
    out.normal_line("[Operating System]");
    let os_result = check_os::check()?;

    out.normal_line("[Python Development Environment]");
    let python_result = check_python::check()?;

    out.normal_line("[Go Development Environment]");
    let go_result = check_go::check()?;

    out.normal_line("[Node.js Development Environment]");
    let nodejs_result = check_nodejs::check()?;

    out.normal_line("[C++ Development Environment]");
    let cpp_result = check_cpp::check()?;

    // Create combined result
    let env_result = EnvCheckResult {
        os: os_result,
        cpp: cpp_result,
        python: python_result,
        go: go_result,
        nodejs: nodejs_result,
    };

    // Format output using the CLI formatter
    let formatter = CliFormatter;

    // Re-print with proper formatting
    out.normal_line("");
    out.normal_line("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    out.normal_line("");
    out.normal_line("[Operating System]");
    formatter.format_os_check(&env_result.os, out.clone());
    out.normal_line("");

    out.normal_line("[Python Development Environment]");
    formatter.format_python_check(&env_result.python, out.clone());
    out.normal_line("");

    out.normal_line("[Go Development Environment]");
    formatter.format_go_check(&env_result.go, out.clone());
    out.normal_line("");

    out.normal_line("[Node.js Development Environment]");
    formatter.format_nodejs_check(&env_result.nodejs, out.clone());
    out.normal_line("");

    out.normal_line("[C++ Development Environment]");
    formatter.format_cpp_check(&env_result.cpp, out.clone());
    out.normal_line("");

    out.normal_line("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    out.normal_line("");

    // Summary
    out.normal_line("📊 Environment Check Summary:");

    // OS
    match env_result.os.status {
        CheckStatus::Ok => {
            out.normal_line("   ✅ Operating System: Supported");
        }
        CheckStatus::Warning => {
            out.normal_line("   ⚠️  Operating System: Not fully supported");
        }
        _ => {
            out.normal_line("   ❌ Operating System: Not supported");
        }
    }

    // Python
    match env_result.python.status {
        CheckStatus::Ok => {
            out.normal_line("   ✅ Python:   Ready");
        }
        CheckStatus::Warning => {
            out.normal_line("   ⚠️  Python:   Version mismatch");
        }
        _ => {
            out.normal_line("   ❌ Python:   Not ready");
        }
    }

    // Go
    match env_result.go.status {
        CheckStatus::Ok => {
            out.normal_line("   ✅ Go:       Ready");
        }
        CheckStatus::Warning => {
            out.normal_line("   ⚠️  Go:       Version too old");
        }
        _ => {
            out.normal_line("   ❌ Go:       Not ready");
        }
    }

    // Node.js
    match env_result.nodejs.status {
        CheckStatus::Ok => {
            out.normal_line("   ✅ Node.js:  Ready");
        }
        CheckStatus::Warning => {
            if env_result.nodejs.has_nodejs && !env_result.nodejs.has_npm {
                out.normal_line("   ⚠️  Node.js:  Partially ready (npm missing)");
            } else {
                out.normal_line("   ⚠️  Node.js:  Outdated version");
            }
        }
        _ => {
            out.normal_line("   ❌ Node.js:  Not ready");
        }
    }

    // C++
    match env_result.cpp.status {
        CheckStatus::Ok => {
            out.normal_line("   ✅ C++:      Ready");
        }
        CheckStatus::Warning => {
            out.normal_line("   ⚠️  C++:      Partially ready");
        }
        _ => {
            out.normal_line("   ❌ C++:      Not ready");
        }
    }

    out.normal_line("");
    out.normal_line("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    out.normal_line("");

    // Final message
    let os_ok = env_result.os.status == CheckStatus::Ok;
    let python_ok = env_result.python.status == CheckStatus::Ok;
    let go_ok = env_result.go.status == CheckStatus::Ok;
    let nodejs_ok = env_result.nodejs.status == CheckStatus::Ok;
    let cpp_ok = env_result.cpp.status == CheckStatus::Ok;

    let all_core_ready = os_ok && python_ok && go_ok && nodejs_ok;
    let cpp_ready = cpp_ok;

    if all_core_ready && cpp_ready {
        out.normal_line("✨ All development environments are ready!");
    } else if all_core_ready {
        out.normal_line("✨ Core development environments are ready!");
        out.normal_line("   You can start developing Python/Go/TypeScript extensions");
        out.normal_line("");
        out.normal_line("⚠️  C++ environment needs additional setup for C++ extension development");
    } else {
        out.normal_line("❌ Some development environments are not ready!");
        out.normal_line("");
        out.normal_line("💡 Installation guide:");
        out.normal_line("   https://theten.ai/docs/getting-started/quick-start");
    }

    Ok(())
}
