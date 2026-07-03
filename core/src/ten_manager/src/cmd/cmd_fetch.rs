//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs, path::PathBuf, sync::Arc, time::Instant};

use anyhow::{anyhow, Context, Result};
use clap::{Arg, ArgAction, ArgMatches, Command};
use console::Emoji;
use indicatif::HumanDuration;
use semver::VersionReq;
use tempfile;
use ten_rust::pkg_info::{
    manifest::support::ManifestSupport,
    pkg_type::PkgType,
    supports::{Arch, Os},
};

use crate::{
    designer::storage::in_memory::TmanStorageInMemory,
    home::config::TmanConfig,
    output::TmanOutput,
    package_file::unpackage::extract_and_process_tpkg_file,
    registry::{get_package, get_package_list},
    version_utils::parse_pkg_name_version_req,
};

#[derive(Debug)]
pub struct FetchCommand {
    pub pkg_type: PkgType,
    pub pkg_name: String,
    pub version_req: VersionReq,
    pub support: ManifestSupport,
    pub output_dir: PathBuf,
    pub no_extract: bool,
}

pub fn create_sub_cmd(args_cfg: &crate::cmd_line::ArgsCfg) -> Command {
    Command::new("fetch")
        .about("Fetch a package from registry")
        .arg(
            Arg::new("PACKAGE_TYPE")
                .help("The type of the package")
                .value_parser(args_cfg.pkg_type.possible_values.clone())
                .required(true),
        )
        .arg(
            Arg::new("PACKAGE_NAME")
                .help("The name of the package with optional version requirement (e.g., foo@1.0.0")
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
            Arg::new("OUTPUT_DIR")
                .long("output-dir")
                .help("Target folder to save the package (or extract into)")
                .default_value(".")
                .required(false),
        )
        .arg(
            Arg::new("NO_EXTRACT")
                .long("no-extract")
                .help("Only fetch the package file without extraction")
                .action(ArgAction::SetTrue)
                .required(false),
        )
}

pub fn parse_sub_cmd(sub_cmd_args: &ArgMatches) -> Result<FetchCommand> {
    let pkg_type_str = sub_cmd_args
        .get_one::<String>("PACKAGE_TYPE")
        .ok_or_else(|| anyhow!("Missing PACKAGE_TYPE"))?;

    let pkg_type = pkg_type_str.parse::<PkgType>()?;

    let pkg_with_version = sub_cmd_args
        .get_one::<String>("PACKAGE_NAME")
        .ok_or_else(|| anyhow!("Missing PACKAGE_NAME"))?;
    let (pkg_name, version_req) = parse_pkg_name_version_req(pkg_with_version)
        .with_context(|| format!("Failed to parse package name '{pkg_with_version}'"))?;

    let support = ManifestSupport {
        os: sub_cmd_args.get_one::<String>("OS").and_then(|s| s.parse::<Os>().ok()),
        arch: sub_cmd_args.get_one::<String>("ARCH").and_then(|s| s.parse::<Arch>().ok()),
    };

    let output_dir = PathBuf::from(sub_cmd_args.get_one::<String>("OUTPUT_DIR").unwrap());

    let no_extract = sub_cmd_args.get_flag("NO_EXTRACT");

    Ok(FetchCommand {
        pkg_type,
        pkg_name,
        version_req,
        support,
        output_dir,
        no_extract,
    })
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    _tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    command_data: FetchCommand,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    let started = Instant::now();

    // Ensure that the target folder exists.
    if !command_data.output_dir.exists() {
        fs::create_dir_all(&command_data.output_dir)?;
    }

    // Query the package from the registry.
    let mut found_packages = get_package_list(
        tman_config.clone(),
        Some(command_data.pkg_type),
        Some(command_data.pkg_name.clone()),
        Some(command_data.version_req.clone()),
        None, // No tag filtering.
        None, // No scope filtering.
        None,
        None, // Retrieve all packages.
        &out,
    )
    .await?;

    if found_packages.is_empty() {
        return Err(anyhow!("Package not found in registry"));
    }

    // Get the latest package.
    found_packages.sort_by(|a, b| b.basic_info.version.cmp(&a.basic_info.version));
    let package = &found_packages[0];
    let package_url = &package.download_url;

    let mut temp_file = tempfile::NamedTempFile::new()?;
    get_package(
        tman_config,
        &package.basic_info.type_and_name.pkg_type,
        &package.basic_info.type_and_name.name,
        &package.basic_info.version,
        package_url,
        &mut temp_file,
        out.clone(),
    )
    .await?;

    if command_data.no_extract {
        // If only the package file is obtained, directly copy the downloaded
        // temporary file to the target location.
        let target_file = command_data.output_dir.join(format!(
            "{}-{}.tpkg",
            package.basic_info.type_and_name.name, package.basic_info.version
        ));

        fs::copy(temp_file.path(), &target_file)?;

        out.normal_line(&format!(
            "{}  Package file saved to '{}'",
            Emoji("📦", ""),
            target_file.display()
        ));
    } else {
        // If `--no-extract` is not specified, extract the files to the target
        // directory.
        let target_path = command_data.output_dir.join(&command_data.pkg_name);
        fs::create_dir_all(&target_path)?;

        extract_and_process_tpkg_file(
            temp_file.path(),
            target_path.to_str().unwrap(),
            // In fetch mode, the template replacement function is not
            // available.
            None,
        )?;

        out.normal_line(&format!(
            "{}  Package extracted to '{}'",
            Emoji("🏆", ""),
            target_path.display()
        ));
    }

    out.normal_line(&format!(
        "{}  Fetch completed in {}.",
        Emoji("✅", ""),
        HumanDuration(started.elapsed())
    ));

    Ok(())
}
