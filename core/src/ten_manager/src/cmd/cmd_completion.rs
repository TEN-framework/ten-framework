//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;
use clap::{Arg, ArgMatches, Command};
use clap_complete::{generate, Shell};

use crate::{
    designer::storage::in_memory::TmanStorageInMemory,
    home::config::{is_verbose, TmanConfig},
    output::TmanOutput,
};

#[derive(Debug)]
pub struct CompletionCommand {
    pub shell: String,
}

pub fn create_sub_cmd(_args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("completion")
        .about("Generate shell completion scripts")
        .arg(
            Arg::new("SHELL")
                .required(true)
                .value_parser(["bash", "zsh", "fish", "powershell"])
                .help("The shell to generate completion for"),
        )
        .after_help(
            "Generate completion scripts for your shell. Example usage:\n\n  tman completion bash \
             > /etc/bash_completion.d/tman\n  tman completion bash > \
             ~/.local/share/bash-completion/completions/tman",
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<CompletionCommand> {
    let shell = sub_cmd_args.get_one::<String>("SHELL").expect("SHELL is required").to_string();

    Ok(CompletionCommand {
        shell,
    })
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    cmd: CompletionCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    if is_verbose(tman_config.clone()).await {
        out.normal_line(&format!("Generating completion for shell: {}", cmd.shell));
    }

    // Create the main command structure for completion generation
    let mut command = create_command_for_completion();

    // Determine the shell type
    let shell = match cmd.shell.as_str() {
        "bash" => Shell::Bash,
        "zsh" => Shell::Zsh,
        "fish" => Shell::Fish,
        "powershell" => Shell::PowerShell,
        _ => return Err(anyhow::anyhow!("Unsupported shell: {}", cmd.shell)),
    };

    // Generate completion and print to stdout
    let mut stdout = std::io::stdout();
    generate(shell, &mut command, "tman", &mut stdout);

    Ok(())
}

/// Create the command structure for completion generation.
/// This should match the structure in cmd_line.rs but without parsing
/// arguments.
fn create_command_for_completion() -> Command {
    let args_cfg = crate::cmd_line::get_args_cfg();

    Command::new("tman")
        .about("TEN manager")
        .disable_version_flag(true)
        // Arguments.
        .arg(
            Arg::new("VERSION")
                .long("version")
                .help("Print version information and check for updates")
                .action(clap::ArgAction::SetTrue),
        )
        .arg(
            Arg::new("CONFIG_FILE")
                .long("config-file")
                .short('c')
                .help("The location of config.json")
                .default_value(None),
        )
        .arg(Arg::new("USER_TOKEN").long("user-token").help("The user token").default_value(None))
        .arg(
            Arg::new("VERBOSE")
                .long("verbose")
                .help("Enable verbose output")
                .action(clap::ArgAction::SetTrue),
        )
        .arg(
            Arg::new("ASSUME_YES")
                .long("yes")
                .short('y')
                .help("Automatically answer 'yes' to all prompts")
                .action(clap::ArgAction::SetTrue),
        )
        // Hidden arguments.
        .arg(
            Arg::new("ADMIN_TOKEN")
                .long("admin-token")
                .help("The administration token")
                .default_value(None)
                .hide(true),
        )
        // Subcommands.
        .subcommand(crate::cmd::cmd_create::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_install::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_fetch::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_uninstall::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_package::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_publish::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_designer::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_check::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_modify::create_sub_cmd(&args_cfg))
        .subcommand(crate::cmd::cmd_run::create_sub_cmd(&args_cfg))
        .subcommand(create_sub_cmd(&args_cfg))
        // Hidden subcommands.
        .subcommand(crate::cmd::cmd_delete::create_sub_cmd(&args_cfg))
}
