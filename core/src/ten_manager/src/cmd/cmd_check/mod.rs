//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod cmd_check_env;
pub mod cmd_check_manifest_json;
pub mod cmd_check_property_json;

use std::sync::Arc;

use anyhow::Result;
use clap::{ArgMatches, Command};

use crate::{
    designer::storage::in_memory::TmanStorageInMemory, home::config::TmanConfig, output::TmanOutput,
};

#[derive(Debug)]
pub enum CheckCommandData {
    CheckEnv(cmd_check_env::CheckEnvCommand),
    CheckManifestJson(cmd_check_manifest_json::CheckManifestJsonCommand),
    CheckPropertyJson(cmd_check_property_json::CheckPropertyJsonCommand),
}

pub fn create_sub_cmd(args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("check")
        .about("Check various consistency validations")
        .subcommand_required(true)
        .arg_required_else_help(true)
        .subcommand(crate::cmd::cmd_check::cmd_check_env::create_sub_cmd(args_cfg))
        .subcommand(crate::cmd::cmd_check::cmd_check_manifest_json::create_sub_cmd(args_cfg))
        .subcommand(crate::cmd::cmd_check::cmd_check_property_json::create_sub_cmd(args_cfg))
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<CheckCommandData> {
    let command_data = match sub_cmd_args.subcommand() {
        Some(("env", env_cmd_args)) => CheckCommandData::CheckEnv(
            crate::cmd::cmd_check::cmd_check_env::parse_sub_cmd(env_cmd_args)?,
        ),
        Some(("manifest-json", manifest_json_cmd_args)) => CheckCommandData::CheckManifestJson(
            crate::cmd::cmd_check::cmd_check_manifest_json::parse_sub_cmd(manifest_json_cmd_args)?,
        ),
        Some(("property-json", property_json_cmd_args)) => CheckCommandData::CheckPropertyJson(
            crate::cmd::cmd_check::cmd_check_property_json::parse_sub_cmd(property_json_cmd_args)?,
        ),

        _ => unreachable!("Command not found"),
    };

    Ok(command_data)
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    command_data: CheckCommandData,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    match command_data {
        CheckCommandData::CheckEnv(cmd) => {
            crate::cmd::cmd_check::cmd_check_env::execute_cmd(
                tman_config,
                tman_storage_in_memory,
                cmd,
                out,
            )
            .await
        }
        CheckCommandData::CheckManifestJson(cmd) => {
            crate::cmd::cmd_check::cmd_check_manifest_json::execute_cmd(
                tman_config,
                tman_storage_in_memory,
                cmd,
                out,
            )
            .await
        }
        CheckCommandData::CheckPropertyJson(cmd) => {
            crate::cmd::cmd_check::cmd_check_property_json::execute_cmd(
                tman_config,
                tman_storage_in_memory,
                cmd,
                out,
            )
            .await
        }
    }
}
