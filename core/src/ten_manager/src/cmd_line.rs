//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;
use clap::{Arg, Command};

use crate::home::config::{read_config, TmanConfig};

pub struct ArgCfg {
    pub possible_values: Vec<&'static str>,
}

pub struct ArgsCfg {
    pub pkg_type: ArgCfg,
    pub os: ArgCfg,
    pub arch: ArgCfg,
    pub build_type: ArgCfg,
    pub _language: ArgCfg,
}

pub struct ParsedCmd {
    pub tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pub command_data: Option<crate::cmd::CommandData>,
    pub show_version: bool,
}

pub fn get_args_cfg() -> ArgsCfg {
    ArgsCfg {
        pkg_type: ArgCfg {
            possible_values: ["system", "protocol", "addon_loader", "extension", "app"].to_vec(),
        },
        os: ArgCfg {
            possible_values: ["linux", "mac", "win"].to_vec(),
        },
        arch: ArgCfg {
            possible_values: ["x86", "x64", "arm", "arm64"].to_vec(),
        },
        build_type: ArgCfg {
            possible_values: ["debug", "release"].to_vec(),
        },
        _language: ArgCfg {
            possible_values: ["cpp", "go"].to_vec(),
        },
    }
}

fn create_cmd() -> clap::ArgMatches {
    let args_cfg = get_args_cfg();

    Command::new("tman")
        .about("TEN manager")
        .disable_version_flag(true)
        .arg_required_else_help(true)
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
        .subcommand(crate::cmd::cmd_completion::create_sub_cmd(&args_cfg))
        // Hidden subcommands.
        .subcommand(crate::cmd::cmd_delete::create_sub_cmd(&args_cfg))
        .get_matches()
}

/// Parses command line arguments and returns a ParsedCmd structure.
///
/// This function processes all command line arguments, reads configuration
/// files, and constructs a TmanConfig with appropriate values. It handles
/// version flags and subcommand parsing.
///
/// Returns a Result containing ParsedCmd or an error if parsing fails.
pub fn parse_cmd() -> Result<ParsedCmd> {
    // Get command line arguments.
    let matches = create_cmd();

    // Create default configuration.
    let default_config = TmanConfig::default();

    // Get config file path from command line or use default.
    let config_file_path = matches.get_one::<String>("CONFIG_FILE").cloned();
    let tman_config_file = read_config(&config_file_path)?;

    // Construct configuration by combining command line args, config file
    // values, and defaults.
    let tman_config = TmanConfig {
        config_file: config_file_path.or(default_config.config_file),
        admin_token: matches.get_one::<String>("ADMIN_TOKEN").cloned().or_else(|| {
            if let Some(tman_config_file) = &tman_config_file {
                tman_config_file.admin_token.clone()
            } else {
                default_config.admin_token
            }
        }),
        user_token: matches.get_one::<String>("USER_TOKEN").cloned().or_else(|| {
            if let Some(tman_config_file) = &tman_config_file {
                tman_config_file.user_token.clone()
            } else {
                default_config.user_token
            }
        }),
        verbose: matches.get_flag("VERBOSE"),
        assume_yes: matches.get_flag("ASSUME_YES"),
        registry: if let Some(tman_config_file) = &tman_config_file {
            tman_config_file.registry.clone()
        } else {
            default_config.registry
        },
        enable_package_cache: default_config.enable_package_cache,
        designer: if let Some(tman_config_file) = &tman_config_file {
            tman_config_file.designer.clone()
        } else {
            default_config.designer
        },
    };

    // Handle version flag separately.
    if matches.get_flag("VERSION") {
        // If `--version` exists, do not parse subcommands.
        return Ok(ParsedCmd {
            tman_config: Arc::new(tokio::sync::RwLock::new(tman_config)),
            command_data: None,
            show_version: true,
        });
    }

    // Parse subcommands and their arguments.
    let command_data = if let Some((subcommand, sub_cmd_args)) = matches.subcommand() {
        match subcommand {
            "create" => crate::cmd::CommandData::Create(crate::cmd::cmd_create::parse_sub_cmd(
                sub_cmd_args,
            )?),
            "install" => crate::cmd::CommandData::Install(crate::cmd::cmd_install::parse_sub_cmd(
                sub_cmd_args,
            )?),
            "fetch" => {
                crate::cmd::CommandData::Fetch(crate::cmd::cmd_fetch::parse_sub_cmd(sub_cmd_args)?)
            }
            "uninstall" => crate::cmd::CommandData::Uninstall(
                crate::cmd::cmd_uninstall::parse_sub_cmd(sub_cmd_args)?,
            ),
            "package" => crate::cmd::CommandData::Package(crate::cmd::cmd_package::parse_sub_cmd(
                sub_cmd_args,
            )?),
            "publish" => crate::cmd::CommandData::Publish(crate::cmd::cmd_publish::parse_sub_cmd(
                sub_cmd_args,
            )?),
            "designer" => crate::cmd::CommandData::Designer(
                crate::cmd::cmd_designer::parse_sub_cmd(sub_cmd_args)?,
            ),
            "check" => {
                crate::cmd::CommandData::Check(crate::cmd::cmd_check::parse_sub_cmd(sub_cmd_args)?)
            }
            "modify" => crate::cmd::CommandData::Modify(crate::cmd::cmd_modify::parse_sub_cmd(
                sub_cmd_args,
            )?),
            "run" => {
                crate::cmd::CommandData::Run(crate::cmd::cmd_run::parse_sub_cmd(sub_cmd_args)?)
            }
            "completion" => crate::cmd::CommandData::Completion(
                crate::cmd::cmd_completion::parse_sub_cmd(sub_cmd_args)?,
            ),
            // Hidden commands.
            "delete" => crate::cmd::CommandData::Delete(crate::cmd::cmd_delete::parse_sub_cmd(
                sub_cmd_args,
            )?),
            _ => unreachable!("Command not found"),
        }
    } else {
        return Err(anyhow::anyhow!("No subcommand provided"));
    };

    // Return the fully constructed ParsedCmd.
    Ok(ParsedCmd {
        tman_config: Arc::new(tokio::sync::RwLock::new(tman_config)),
        command_data: Some(command_data),
        show_version: false,
    })
}
