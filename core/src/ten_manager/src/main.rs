//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(target_os = "windows")]
use std::thread;
use std::{process, sync::Arc};

use anyhow::Result;
use console::Emoji;
// use ten_manager::memory_stats::print_memory_stats;
use ten_manager::output::cli::TmanOutputCli;
use ten_manager::{
    cmd::execute_cmd,
    cmd_line,
    constants::GITHUB_RELEASE_PAGE,
    designer::storage::in_memory::TmanStorageInMemory,
    output::TmanOutput,
    tracing_config::{init_tracing, TracingMode},
    version::VERSION,
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
    run_main_thread(out);
}

fn run(out: Arc<Box<dyn TmanOutput>>) {
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

    // Initialize tracing system based on --tracing parameter
    let tracing_mode_str =
        parsed_cmd.tman_config.try_read().ok().and_then(|config| config.tracing_mode.clone());
    let tracing_mode = TracingMode::from_config(tracing_mode_str.as_deref());
    let _tracing_guard = match init_tracing(tracing_mode, out.clone()) {
        Ok(guard) => guard,
        Err(e) => {
            out.error_line(&format!("{}  Failed to initialize tracing: {}", Emoji("🔴", ":-("), e));
            None
        }
    };

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

    // Explicitly drop tracing_guard before process::exit to ensure proper cleanup
    // process::exit() terminates immediately without running destructors
    drop(_tracing_guard);

    if let Err(e) = result {
        out.error_line(&format!("{}  Error: {:?}", Emoji("🔴", ":-("), e));
        process::exit(1);
    }

    process::exit(0);
}

#[cfg(target_os = "windows")]
fn run_main_thread(out: Arc<Box<dyn TmanOutput>>) {
    const TMAN_MAIN_THREAD_STACK_SIZE_BYTES: usize = 32 * 1024 * 1024;
    let builder = thread::Builder::new()
        .stack_size(TMAN_MAIN_THREAD_STACK_SIZE_BYTES)
        .name("tman-main".into());

    let thread_out = out.clone();
    match builder.spawn(move || run(thread_out)) {
        Ok(handle) => {
            if let Err(err) = handle.join() {
                out.error_line(&format!(
                    "{}  Error: failed to run tman main thread: {:?}",
                    Emoji("🔴", ":-("),
                    err
                ));
                process::exit(1);
            }
        }
        Err(err) => {
            out.error_line(&format!(
                "{}  Error: failed to spawn tman main thread: {}",
                Emoji("🔴", ":-("),
                err
            ));
            process::exit(1);
        }
    }
}

#[cfg(not(target_os = "windows"))]
fn run_main_thread(out: Arc<Box<dyn TmanOutput>>) {
    run(out);
}
