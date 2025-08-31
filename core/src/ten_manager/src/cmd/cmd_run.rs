//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    process::{Command as StdCommand, Stdio},
    sync::Arc,
};

use anyhow::{anyhow, Result};
use clap::{Arg, ArgMatches, Command};
use ten_rust::{pkg_info::constants::MANIFEST_JSON_FILENAME, utils::fs::read_file_to_string};
use tokio::io::AsyncBufReadExt;

use crate::{
    constants::SCRIPTS,
    designer::storage::in_memory::TmanStorageInMemory,
    home::config::{is_verbose, TmanConfig},
    output::TmanOutput,
};

#[derive(Debug)]
pub struct RunCommand {
    pub script_name: String,

    // Stores additional arguments passed after `--`.
    pub extra_args: Vec<String>,
}

pub fn create_sub_cmd(_args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("run")
        .about("Run a custom script from the manifest.json's scripts field")
        .arg(
            Arg::new("SCRIPT_NAME")
                .required(true)
                .help("Which script to run, e.g. 'start' or 'stop'"),
        )
        .arg(
            Arg::new("EXTRA_ARGS")
                .help("Additional arguments to pass to the script")
                .last(true)
                .allow_hyphen_values(true)
                .action(clap::ArgAction::Append)
                .required(false),
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<RunCommand> {
    let script_name =
        sub_cmd_args.get_one::<String>("SCRIPT_NAME").expect("SCRIPT_NAME is required").to_string();

    let extra_args = sub_cmd_args
        .get_many::<String>("EXTRA_ARGS")
        .map(|vals| vals.map(|s| s.to_string()).collect())
        .unwrap_or_default();

    Ok(RunCommand {
        script_name,
        extra_args,
    })
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    cmd: RunCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    if is_verbose(tman_config.clone()).await {
        out.normal_line(&format!("Executing run command: {cmd:?}"));
    }

    // Read `manifest.json` in the current working directory.
    let cwd = crate::fs::get_cwd()?;
    let manifest_path = cwd.join(MANIFEST_JSON_FILENAME);
    if !manifest_path.exists() {
        return Err(anyhow!(
            "{} not found in current directory: {}",
            MANIFEST_JSON_FILENAME,
            manifest_path.display()
        ));
    }

    // Parse `manifest.json`.
    let manifest_json_str = read_file_to_string(&manifest_path)
        .map_err(|e| anyhow!("Failed to read {}: {}", MANIFEST_JSON_FILENAME, e))?;
    let manifest_value: serde_json::Value = serde_json::from_str(&manifest_json_str)
        .map_err(|e| anyhow!("Failed to parse {}: {}", MANIFEST_JSON_FILENAME, e))?;

    // Get `scripts`.
    let scripts = match manifest_value.get(SCRIPTS) {
        Some(s) => s.clone(),
        None => {
            return Err(anyhow!("No 'scripts' field found in {}", manifest_path.display()));
        }
    };

    if !scripts.is_object() {
        return Err(anyhow!("'{}' in {} must be an object", SCRIPTS, MANIFEST_JSON_FILENAME));
    }

    // Find the specified script_name.
    let scripts_obj = scripts.as_object().unwrap();
    let script_cmd = match scripts_obj.get(&cmd.script_name) {
        Some(v) => v.as_str().ok_or_else(|| {
            anyhow!(
                "The script '{}' is not a string in {}",
                &cmd.script_name,
                MANIFEST_JSON_FILENAME
            )
        })?,
        None => {
            return Err(anyhow!("Script '{}' not found in '{}'", &cmd.script_name, SCRIPTS));
        }
    };

    // Execute the subprocess.
    if is_verbose(tman_config.clone()).await {
        out.normal_line(&format!("About to run script: {} -> {}", &cmd.script_name, script_cmd));
    }

    // Determine whether it is a shell command.
    #[cfg(windows)]
    let is_shell_cmd = script_cmd.to_lowercase().starts_with("cmd.exe /c");
    #[cfg(not(windows))]
    let is_shell_cmd = script_cmd.starts_with("sh -c");

    let mut command_builder = if is_shell_cmd {
        // If it is a shell command, do not split by spaces; extract the shell
        // command and its parameters.
        #[cfg(windows)]
        {
            let prefix = "cmd.exe /c";

            // Extract the command body: remove the prefix and trim the leading
            // spaces from the remaining part.
            let command_body = script_cmd[prefix.len()..].trim();
            if command_body.is_empty() {
                return Err(anyhow!("No command provided after '{}'", prefix));
            }

            let mut cmd_builder = StdCommand::new("cmd.exe");

            // Construct the parameter list.
            let mut args = vec!["/C".to_string(), command_body.to_string()];

            // Append additional parameters, if any.
            for extra in &cmd.extra_args {
                args.push(extra.to_string());
            }

            cmd_builder.args(args);
            cmd_builder
        }
        #[cfg(not(windows))]
        {
            let prefix = "sh -c";

            let command_body = script_cmd[prefix.len()..].trim();
            if command_body.is_empty() {
                return Err(anyhow!("No command provided after '{}'", prefix));
            }

            let mut cmd_builder = StdCommand::new("sh");

            // Construct the parameter list.
            let mut args = vec!["-c".to_string(), command_body.to_string()];

            // Append additional parameters, if any.
            for extra in &cmd.extra_args {
                args.push(extra.to_string());
            }

            cmd_builder.args(args);
            cmd_builder
        }
    } else {
        // For non-shell commands, split the command and parameters by spaces.
        let mut parts = script_cmd.split_whitespace();
        let exec = parts.next().unwrap();
        let mut args: Vec<String> = parts.map(|s| s.to_string()).collect();

        // Append additional parameters, if any.
        for arg in &cmd.extra_args {
            args.push(arg.to_string());
        }

        let mut cmd_builder = StdCommand::new(exec);

        cmd_builder.args(args);
        cmd_builder
    };

    // Get the standard output and standard error of the subprocess.
    command_builder.stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());

    // Spawn the subprocess.
    let mut child = tokio::process::Command::from(command_builder)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| anyhow!("Failed to spawn subprocess: {}", e))?;

    // Async reading of stdout.
    if let Some(stdout) = child.stdout.take() {
        let mut reader = tokio::io::BufReader::new(stdout).lines();
        let out_clone = out.clone();
        tokio::spawn(async move {
            while let Ok(Some(line)) = reader.next_line().await {
                out_clone.normal_line(&line);
            }
        });
    }

    // Async reading of stderr.
    if let Some(stderr) = child.stderr.take() {
        let mut reader_err = tokio::io::BufReader::new(stderr).lines();
        let out_clone = out.clone();
        tokio::spawn(async move {
            while let Ok(Some(line)) = reader_err.next_line().await {
                out_clone.error_line(&line);
            }
        });
    }

    // Await child exit asynchronously.
    let status =
        child.wait().await.map_err(|e| anyhow!("Failed to wait for the subprocess: {}", e))?;

    if !status.success() {
        return Err(anyhow!(
            "Script '{}' exited with non-zero code: {:?}",
            &cmd.script_name,
            status.code()
        ));
    }

    Ok(())
}
