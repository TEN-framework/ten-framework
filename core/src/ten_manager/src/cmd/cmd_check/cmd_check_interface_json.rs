//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::Result;
use clap::{Arg, ArgMatches, Command};
use console::Emoji;
use ten_rust::json_schema::ten_validate_interface_json_file;

use crate::{
    designer::storage::in_memory::TmanStorageInMemory, home::config::TmanConfig, output::TmanOutput,
};

#[derive(Debug)]
pub struct CheckInterfaceJsonCommand {
    pub path: String,
}

pub fn create_sub_cmd(_args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("interface")
        .about("Check if an interface JSON file is valid according to the json schema")
        .arg(
            Arg::new("PATH")
                .long("path")
                .help("The file path of interface JSON file to be checked")
                .required(true)
                .num_args(1),
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<CheckInterfaceJsonCommand> {
    let cmd = CheckInterfaceJsonCommand {
        path: sub_cmd_args.get_one::<String>("PATH").cloned().unwrap(),
    };

    Ok(cmd)
}

pub async fn execute_cmd(
    _tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    command_data: CheckInterfaceJsonCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    match ten_validate_interface_json_file(&command_data.path) {
        Ok(_) => {
            out.normal_line(&format!("{}  Conforms to JSON schema.", Emoji("👍", "Passed")));
            Ok(())
        }
        Err(e) => Err(e),
    }
}
