//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc, time::Instant};

use anyhow::{anyhow, Context, Result};
use clap::{Arg, ArgAction, ArgMatches, Command};
use console::Emoji;
use indicatif::HumanDuration;
use semver::VersionReq;
use ten_rust::pkg_info::{
    manifest::support::ManifestSupport,
    pkg_type::PkgType,
    supports::{Arch, Os},
};

use crate::{
    create::create_pkg_in_path, designer::storage::in_memory::TmanStorageInMemory,
    home::config::TmanConfig, output::TmanOutput, version_utils::parse_pkg_name_version_req,
};

#[derive(Debug)]
pub struct CreateCommand {
    pub pkg_type: PkgType,
    pub pkg_name: String,
    pub support: ManifestSupport,
    pub template_name: String,
    pub template_version_req: VersionReq,
    pub template_data: HashMap<String, String>,
}

pub fn create_sub_cmd(args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("create")
        .about("Create a package")
        .arg(
            Arg::new("PACKAGE_TYPE")
                .help("The type of the package")
                .value_parser(args_cfg.pkg_type.possible_values.clone())
                .required(true),
        )
        .arg(
            Arg::new("PACKAGE_NAME")
                .help("The name of the package with optional version requirement (e.g., foo@1.0.0)")
                .required(true),
        )
        .arg(
            Arg::new("OS")
                .long("os")
                .help("The operating system")
                .value_parser(args_cfg.os.possible_values.clone())
                .required(false),
        )
        .arg(
            Arg::new("ARCH")
                .long("arch")
                .help("The CPU architecture")
                .value_parser(args_cfg.arch.possible_values.clone())
                .required(false),
        )
        .arg(
            Arg::new("TEMPLATE")
                .long("template")
                .help("The template package to create from")
                .required(true),
        )
        .arg(
            Arg::new("TEMPLATE_DATA")
                .long("template-data")
                .help(
                    "The placeholders used within the template and their corresponding values. \
                     The format is key-value pairs, e.g., `--template-data key=value`",
                )
                .value_name("KEY=VALUE")
                .action(ArgAction::Append),
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<CreateCommand> {
    let pkg_type_str = sub_cmd_args
        .get_one::<String>("PACKAGE_TYPE")
        .cloned()
        .ok_or_else(|| anyhow!("Missing required argument: PACKAGE_TYPE"))?;
    let pkg_type = pkg_type_str.parse::<PkgType>().context("Invalid PACKAGE_TYPE format")?;

    let pkg_name = sub_cmd_args
        .get_one::<String>("PACKAGE_NAME")
        .cloned()
        .ok_or_else(|| anyhow!("Missing required argument: PACKAGE_NAME"))?;

    let os = sub_cmd_args.get_one::<String>("OS").and_then(|s| s.parse::<Os>().ok());

    let arch = sub_cmd_args.get_one::<String>("ARCH").and_then(|s| s.parse::<Arch>().ok());

    let support = ManifestSupport {
        os,
        arch,
    };

    let mut cmd = CreateCommand {
        pkg_type,
        pkg_name,
        support,
        template_name: String::new(),
        template_version_req: VersionReq::default(),
        template_data: HashMap::new(),
    };

    let _ = cmd.support.set_defaults();

    if let Some(kv_pairs) = sub_cmd_args.get_many::<String>("TEMPLATE_DATA") {
        for pair in kv_pairs {
            let mut split = pair.splitn(2, '=');
            if let (Some(key), Some(value)) = (split.next(), split.next()) {
                cmd.template_data.insert(key.to_string(), value.to_string());
            }
        }
    }

    if cmd.template_data.contains_key("package_name") {
        return Err(anyhow!(
            "The 'package_name' is set via the command line as '{}', and cannot be modified \
             through '--template-data'.",
            cmd.pkg_name
        ));
    }

    cmd.template_data.insert("package_name".to_string(), cmd.pkg_name.clone());

    let template = sub_cmd_args
        .get_one::<String>("TEMPLATE")
        .cloned()
        .ok_or_else(|| anyhow!("Missing required argument: TEMPLATE"))?;

    let (parsed_name, parsed_version_req) = parse_pkg_name_version_req(&template)
        .with_context(|| format!("Failed to parse template '{template}'"))?;

    cmd.template_name = parsed_name;
    cmd.template_version_req = parsed_version_req;

    Ok(cmd)
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    command_data: CreateCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    let started = Instant::now();

    // Retrieve the current working directory.
    let cwd = std::env::current_dir().context("Failed to get current working directory")?;

    create_pkg_in_path(
        tman_config,
        &cwd,
        &command_data.pkg_type,
        &command_data.pkg_name,
        &command_data.template_name,
        &command_data.template_version_req,
        Some(&command_data.template_data),
        &out,
    )
    .await
    .context("Failed to create the package")?;

    out.normal_line(&format!(
        "{}  Package '{}:{}' created successfully in '{}' in {}.",
        Emoji("🏆", ":-)"),
        command_data.pkg_type,
        command_data.pkg_name,
        cwd.display(),
        HumanDuration(started.elapsed())
    ));

    Ok(())
}
