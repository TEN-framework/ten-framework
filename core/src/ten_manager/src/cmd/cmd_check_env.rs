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
    check_env::{check_cpp, check_go, check_nodejs, check_os, check_python},
    designer::storage::in_memory::TmanStorageInMemory,
    home::config::TmanConfig,
    output::TmanOutput,
};

#[derive(Debug)]
pub struct CheckEnvCommand {}

pub fn create_sub_cmd(_args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("check_env").about("Check development environment for TEN Framework").after_help(
        "Check if your system has the required development environments:\n\n  - Operating System \
         (Linux/macOS x64/arm64)\n  - Python 3.8+\n  - Go 1.20+\n  - Node.js and npm\n  - C++ \
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

    // Check Operating System
    out.normal_line("[Operating System]");
    let os_supported = check_os::check(out.clone())?;
    out.normal_line("");

    // Check Python Development Environment
    out.normal_line("[Python Development Environment]");
    let python_ok = check_python::check(out.clone())?;
    out.normal_line("");

    // Check Go Development Environment
    out.normal_line("[Go Development Environment]");
    let go_ok = check_go::check(out.clone())?;
    out.normal_line("");

    // Check Node.js Development Environment
    out.normal_line("[Node.js Development Environment]");
    let (nodejs_ok, npm_ok) = check_nodejs::check(out.clone())?;
    out.normal_line("");

    // Check C++ Development Environment
    out.normal_line("[C++ Development Environment]");
    let (tgn_ok, cpp_compiler_ok) = check_cpp::check(out.clone())?;
    out.normal_line("");

    out.normal_line("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    out.normal_line("");

    // Summary
    out.normal_line("📊 Environment Check Summary:");

    // OS
    if os_supported {
        out.normal_line("   ✅ Operating System: Supported");
    } else {
        out.normal_line("   ❌ Operating System: Not supported");
    }

    // Python
    if python_ok {
        out.normal_line("   ✅ Python:   Ready");
    } else {
        out.normal_line("   ❌ Python:   Not ready");
    }

    // Go
    if go_ok {
        out.normal_line("   ✅ Go:       Ready");
    } else {
        out.normal_line("   ❌ Go:       Not ready");
    }

    // Node.js
    if nodejs_ok && npm_ok {
        out.normal_line("   ✅ Node.js:  Ready");
    } else if nodejs_ok {
        out.normal_line("   ⚠️  Node.js:  Partially ready (npm missing)");
    } else {
        out.normal_line("   ❌ Node.js:  Not ready");
    }

    // C++
    if tgn_ok && cpp_compiler_ok {
        out.normal_line("   ✅ C++:      Ready");
    } else if tgn_ok || cpp_compiler_ok {
        out.normal_line("   ⚠️  C++:      Partially ready");
    } else {
        out.normal_line("   ❌ C++:      Not ready");
    }

    out.normal_line("");
    out.normal_line("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    out.normal_line("");

    // Final message
    let all_core_ready = os_supported && python_ok && go_ok && nodejs_ok && npm_ok;
    let cpp_ready = tgn_ok && cpp_compiler_ok;

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
