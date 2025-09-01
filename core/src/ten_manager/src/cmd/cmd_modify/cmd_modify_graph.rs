//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs, path::PathBuf, sync::Arc};

use anyhow::{Context, Result};
use clap::{Arg, ArgMatches, Command};
use console::Emoji;
use serde_json::Value;
use ten_rust::{pkg_info::constants::PROPERTY_JSON_FILENAME, utils::fs::read_file_to_string};

use crate::{
    cmd::cmd_modify::jq_util::jq_run, designer::storage::in_memory::TmanStorageInMemory,
    home::config::TmanConfig, output::TmanOutput,
};

#[derive(Debug)]
pub struct ModifyGraphCommand {
    pub app_dir: String,
    pub predefined_graph_name: String,
    pub modification: String,
    pub inplace: bool,
}

pub fn create_sub_cmd(_args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("graph")
        .about("Modify a specified predefined graph in property.json")
        .arg(
            Arg::new("APP_DIR")
                .long("app-dir")
                .help("Specify the app directory")
                .required(true)
                .num_args(1),
        )
        .arg(
            Arg::new("PREDEFINED_GRAPH_NAME")
                .long("predefined-graph-name")
                .help("Specify the predefined graph name to be modified")
                .required(true)
                .num_args(1),
        )
        .arg(
            Arg::new("MODIFICATION")
                .long("modification")
                .short('m')
                .help("The path=JsonString to modify in the selected graph. E.g. .name=\"test\"")
                .required(true)
                .num_args(1),
        )
        .arg(
            Arg::new("INPLACE")
                .long("inplace")
                .short('i')
                .help("Overwrite the original property.json file")
                .required(false)
                .action(clap::ArgAction::SetTrue),
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<ModifyGraphCommand> {
    let cmd = ModifyGraphCommand {
        app_dir: sub_cmd_args.get_one::<String>("APP_DIR").unwrap().to_string(),
        predefined_graph_name: sub_cmd_args
            .get_one::<String>("PREDEFINED_GRAPH_NAME")
            .unwrap()
            .to_string(),
        modification: sub_cmd_args.get_one::<String>("MODIFICATION").unwrap().to_string(),
        inplace: sub_cmd_args.get_flag("INPLACE"),
    };

    Ok(cmd)
}

pub async fn execute_cmd(
    _tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    command_data: ModifyGraphCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    // Find `property.json`.
    let property_file_path = PathBuf::from(&command_data.app_dir).join(PROPERTY_JSON_FILENAME);
    let property_str = read_file_to_string(&property_file_path)
        .with_context(|| format!("Failed to read file: {property_file_path:?}"))?;

    // Parse `property.json`.
    let mut property_json: Value = serde_json::from_str(&property_str)
        .with_context(|| format!("Failed to parse {PROPERTY_JSON_FILENAME} into JSON"))?;

    // Find the specified predefined_graph.
    let graphs_val = property_json
        .pointer_mut("/ten/predefined_graphs")
        .ok_or_else(|| anyhow::anyhow!("No predefined_graphs in {}", PROPERTY_JSON_FILENAME))?;

    let graphs = graphs_val.as_array_mut().ok_or_else(|| {
        anyhow::anyhow!("predefined_graphs in {} is not an array", PROPERTY_JSON_FILENAME)
    })?;

    let mut target_graph_opt = None;
    for g in graphs.iter_mut() {
        if let Some(name_val) = g.get("name") {
            if name_val == command_data.predefined_graph_name.as_str() {
                target_graph_opt = Some(g);
                break;
            }
        }
    }

    let target_graph = target_graph_opt.ok_or_else(|| {
        anyhow::anyhow!(
            "Predefined graph with name='{}' not found",
            command_data.predefined_graph_name
        )
    })?;

    let output = jq_run(target_graph.clone(), &command_data.modification).unwrap();

    if !command_data.inplace {
        out.normal_line(&serde_json::to_string_pretty(&output)?);
        return Ok(());
    }

    // If inplace is true, overwrite the original property.json file.

    // Update target_graph with the output.
    *target_graph = output.clone();

    let new_property_str = serde_json::to_string_pretty(&property_json)?;
    fs::write(&property_file_path, new_property_str)?;

    out.normal_line(&format!(
        "{}  Successfully modified the graph '{}'",
        Emoji("🏆", ":-)"),
        command_data.predefined_graph_name
    ));

    Ok(())
}
