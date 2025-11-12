//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{path::PathBuf, sync::Arc, time::Instant};

use anyhow::Result;
use clap::{Arg, ArgMatches, Command};
use console::Emoji;
use indicatif::HumanDuration;
use ten_rust::pkg_info::get_pkg_info_from_path;

use crate::{
    constants::{DOT_TEN_DIR, PACKAGE_DIR_IN_DOT_TEN_DIR},
    designer::storage::in_memory::TmanStorageInMemory,
    home::config::{is_verbose, TmanConfig},
    output::TmanOutput,
    package_file::{create_package_tar_gz_file, get_tpkg_file_name},
};

#[derive(Debug)]
pub struct PackageCommand {
    pub output_path: Option<String>,
    pub get_identity: bool,
}

pub fn create_sub_cmd(_args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("package")
        .about("Create a package file")
        .arg(
            Arg::new("OUTPUT_PATH")
                .long("output-path")
                .help("Specify the output file path for the package")
                .required(false),
        )
        // Hidden argments.
        .arg(
            Arg::new("GET_IDENTITY")
                .long("get-identity")
                .help("Output the package identity (type, name, version, hash) to stdout and exit")
                .action(clap::ArgAction::SetTrue)
                .hide(true)
                .required(false),
        )
        .after_help(
            "Switch to the base directory of the TEN package you want to package, then simply run \
             'tman package' directly in that directory.",
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<crate::cmd::cmd_package::PackageCommand> {
    let output_path = sub_cmd_args.get_one::<String>("OUTPUT_PATH").cloned();
    let get_identity = sub_cmd_args.get_flag("GET_IDENTITY");
    Ok(crate::cmd::cmd_package::PackageCommand {
        output_path,
        get_identity,
    })
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    command_data: PackageCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    if is_verbose(tman_config.clone()).await {
        out.normal_line("Executing package command");
        out.normal_line(&format!("{command_data:?}"));
    }

    let cwd = crate::fs::get_cwd()?;

    if command_data.get_identity {
        // Get the package info and output identity information.
        let pkg_info = get_pkg_info_from_path(&cwd, true, false, &mut None, None).await?;
        let hash = pkg_info.gen_hash_hex();

        let pkg_type = &pkg_info.manifest.type_and_name.pkg_type;
        let name = &pkg_info.manifest.type_and_name.name;
        let version = &pkg_info.manifest.version;

        println!("{pkg_type} {name} {version} {hash}");

        return Ok(());
    }

    let started = Instant::now();

    let output_path: PathBuf;
    if let Some(command_data_output_path) = &command_data.output_path {
        output_path = std::path::PathBuf::from(command_data_output_path);
    } else {
        // Use the default output path, which is located in the `.ten/`
        // directory, ensuring that under normal circumstances, it will not be
        // uploaded to the git repository.
        let pkg_info = get_pkg_info_from_path(&cwd, true, false, &mut None, None).await?;
        let output_pkg_file_name = get_tpkg_file_name(&pkg_info)?;

        // Create the output directory.
        let output_dir = cwd.join(DOT_TEN_DIR).join(PACKAGE_DIR_IN_DOT_TEN_DIR);
        if !output_dir.exists() {
            std::fs::create_dir_all(&output_dir)?;
        }

        output_path = output_dir.join(output_pkg_file_name);
    }

    if output_path.exists() {
        std::fs::remove_file(&output_path)?;
    }

    let output_path_str =
        create_package_tar_gz_file(tman_config, &output_path, &cwd, out.clone()).await?;

    out.normal_line(&format!(
        "{}  Pack package to {:?} in {}",
        Emoji("🏆", ":-)"),
        output_path_str,
        HumanDuration(started.elapsed())
    ));

    Ok(())
}
