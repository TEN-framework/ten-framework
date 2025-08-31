//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{process, sync::Arc};

use anyhow::Result;
use console::Emoji;
// use ten_manager::memory_stats::print_memory_stats;
use ten_manager::output::cli::TmanOutputCli;
use ten_manager::{
    cmd::execute_cmd, cmd_line, constants::GITHUB_RELEASE_PAGE,
    designer::storage::in_memory::TmanStorageInMemory, output::TmanOutput, version::VERSION,
    version_utils::check_update,
};
use tokio::runtime::Runtime;

fn check_update_from_cmdline(out: Arc<Box<dyn TmanOutput>>) -> Result<()> {
    out.normal_line("Checking for new version...");

    let rt = Runtime::new()?;

    match rt.block_on(check_update()) {
        Ok((true, latest)) => {
            out.normal_line(&format!(
                "New version found: {latest}. Please go to {GITHUB_RELEASE_PAGE} to download the \
                 update."
            ));
        }
        Ok((false, _)) => {
            out.normal_line("Already up to date.");
        }
        Err(err_msg) => {
            out.normal_line(&err_msg.to_string());
        }
    }

    Ok(())
}

fn main() {
    let out: Arc<Box<dyn TmanOutput>> = Arc::new(Box::new(TmanOutputCli));

    let parsed_cmd = match cmd_line::parse_cmd() {
        Ok(parsed_cmd) => parsed_cmd,
        Err(e) => {
            out.error_line(&format!("{}  Error: {}", Emoji("🔴", ":-("), e));
            process::exit(1);
        }
    };

    if parsed_cmd.show_version {
        out.normal_line(&format!("TEN Framework version: {VERSION}"));

        // print_memory_stats("at version check");

        // Call the update check function
        match check_update_from_cmdline(out.clone()) {
            Ok(_) => {
                // If `--version` is passed, ignore other parameters and exit
                // directly.
                process::exit(0);
            }
            Err(e) => {
                out.error_line(&format!("{}  Error: {}", Emoji("🔴", ":-("), e));
                process::exit(1);
            }
        }
    }

    let rt = match Runtime::new() {
        Ok(rt) => rt,
        Err(e) => {
            out.error_line(&format!("{}  Error: {}", Emoji("🔴", ":-("), e));
            process::exit(1);
        }
    };

    let result = rt.block_on(execute_cmd(
        parsed_cmd.tman_config,
        Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        parsed_cmd.command_data.unwrap(),
        out.clone(),
    ));

    // print_memory_stats("at program end");

    if let Err(e) = result {
        out.error_line(&format!("{}  Error: {:?}", Emoji("🔴", ":-("), e));
        process::exit(1);
    }

    process::exit(0);
}
