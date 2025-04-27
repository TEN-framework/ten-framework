//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod cmd_check_manifest_json;
pub mod cmd_check_property_json;

use std::sync::Arc;

use anyhow::Result;
use clap::{ArgMatches, Command};

use crate::{
    config::{metadata::TmanMetadata, TmanConfig},
    output::TmanOutput,
};

#[derive(Debug)]
pub enum CheckCommandData {
    CheckManifestJson(cmd_check_manifest_json::CheckManifestJsonCommand),
    CheckPropertyJson(cmd_check_property_json::CheckPropertyJsonCommand),
}

pub fn create_sub_cmd(args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("check")
        .about("Check various consistency validations")
        .subcommand_required(true)
        .arg_required_else_help(true)
        .subcommand(
            crate::cmd::cmd_check::cmd_check_manifest_json::create_sub_cmd(
                args_cfg,
            ),
        )
        .subcommand(
            crate::cmd::cmd_check::cmd_check_property_json::create_sub_cmd(
                args_cfg,
            ),
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<CheckCommandData> {
    let command_data = match sub_cmd_args.subcommand() {
        Some(("manifest-json", manifest_json_cmd_args)) => {
            CheckCommandData::CheckManifestJson(
                crate::cmd::cmd_check::cmd_check_manifest_json::parse_sub_cmd(
                    manifest_json_cmd_args,
                )?,
            )
        }
        Some(("property-json", property_json_cmd_args)) => {
            CheckCommandData::CheckPropertyJson(
                crate::cmd::cmd_check::cmd_check_property_json::parse_sub_cmd(
                    property_json_cmd_args,
                )?,
            )
        }

        _ => unreachable!("Command not found"),
    };

    Ok(command_data)
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    tman_metadata: Arc<tokio::sync::RwLock<TmanMetadata>>,
    command_data: CheckCommandData,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    match command_data {
        CheckCommandData::CheckManifestJson(cmd) => {
            crate::cmd::cmd_check::cmd_check_manifest_json::execute_cmd(
                tman_config,
                tman_metadata,
                cmd,
                out,
            )
            .await
        }
        CheckCommandData::CheckPropertyJson(cmd) => {
            crate::cmd::cmd_check::cmd_check_property_json::execute_cmd(
                tman_config,
                tman_metadata,
                cmd,
                out,
            )
            .await
        }
    }
}
