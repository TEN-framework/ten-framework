//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, path::Path, sync::Arc};

use anyhow::Result;
use console::Emoji;
use indicatif::{ProgressBar, ProgressStyle};
use regex::Regex;
use semver::Version;
use ten_rust::pkg_info::{
    constants::{ADDON_LOADER_DIR, EXTENSION_DIR, PROTOCOL_DIR, SYSTEM_DIR, TEN_PACKAGES_DIR},
    pkg_basic_info::PkgBasicInfo,
    pkg_type::PkgType,
    pkg_type_and_name::PkgTypeAndName,
    PkgInfo,
};

use crate::{
    cmd::cmd_install::InstallCommand, home::config::TmanConfig, install::install_pkg_info,
    output::TmanOutput,
};

pub fn extract_solver_results_from_raw_solver_results(
    results: &[String],
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
) -> Result<Vec<PkgInfo>> {
    let re = Regex::new(r#"selected_pkg_version\("([^"]+)","([^"]+)","([^"]+)"\)"#).unwrap();

    let mut results_info: Vec<PkgInfo> = vec![];

    'outer: for result in results.iter() {
        if let Some(caps) = re.captures(result) {
            let pkg_type_str = caps.get(1).map_or("", |m| m.as_str());
            let name = caps.get(2).map_or("", |m| m.as_str());
            let semver_str = caps.get(3).map_or("", |m| m.as_str());

            let pkg_type = pkg_type_str.parse::<PkgType>()?;
            let semver = semver_str.parse::<Version>()?;

            for candidate in all_candidates
                .get(&PkgTypeAndName {
                    pkg_type,
                    name: name.to_string(),
                })
                .unwrap()
            {
                if candidate.1.manifest.type_and_name.pkg_type != pkg_type
                    || candidate.1.manifest.type_and_name.name != name
                {
                    panic!("Should not happen.");
                }

                if candidate.1.manifest.version == semver {
                    results_info.push(candidate.1.clone());
                    continue 'outer;
                }
            }
        }
    }

    Ok(results_info)
}

pub fn filter_solver_results_by_type_and_name<'a>(
    solver_results: &'a [PkgInfo],
    pkg_type: Option<&PkgType>,
    name: Option<&String>,
    filter_in: bool,
) -> Result<Vec<&'a PkgInfo>> {
    let mut filtered_results: Vec<&PkgInfo> = vec![];

    for result in solver_results.iter() {
        // After Rust 1.82, there is an `is_none_or` method, and Clippy will
        // suggest using `is_none_or` for simplicity. However, since Ten
        // currently needs to rely on Rust 1.81 on macOS (mainly because debug
        // mode on Mac enables ASAN, and rustc must match the versions of
        // LLVM/Clang to prevent ASAN linking errors), we disable this specific
        // Clippy warning here.
        #[allow(clippy::unnecessary_map_or)]
        let matches_type =
            pkg_type.map_or(true, |pt| result.manifest.type_and_name.pkg_type == *pt);

        #[allow(clippy::unnecessary_map_or)]
        let matches_name = name.map_or(true, |n| result.manifest.type_and_name.name == *n);

        let matches = matches_type && matches_name;

        if filter_in {
            if matches {
                filtered_results.push(result);
            }
        } else if !matches {
            filtered_results.push(result);
        }
    }

    Ok(filtered_results)
}

pub async fn install_solver_results_in_app_folder(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    command_data: &InstallCommand,
    solver_results: &Vec<&PkgInfo>,
    app_dir: &Path,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    out.normal_line(&format!("{}  Installing packages...", Emoji("📥", "+")));

    let bar = if out.is_interactive() {
        Some(ProgressBar::new(solver_results.len().try_into()?))
    } else {
        None
    };

    if let Some(bar) = &bar {
        bar.set_style(
            ProgressStyle::default_bar()
                .template(
                    "{spinner:.green} [{elapsed_precise}] [{bar:40.cyan/blue}] {pos:>7}/{len:7} \
                     {msg}",
                )?
                .progress_chars("#>-"),
        );
    }

    // Prepare progress tracking for parallel execution.
    let progress_counter = Arc::new(std::sync::atomic::AtomicUsize::new(0));
    let total_pkgs = solver_results.len();

    // Prepare futures for all packages.
    let install_futures = solver_results
        .iter()
        .map(|solver_result| {
            let tman_config = tman_config.clone();
            let out = out.clone();
            let base_dir = match solver_result.manifest.type_and_name.pkg_type {
                PkgType::Extension => app_dir.join(TEN_PACKAGES_DIR).join(EXTENSION_DIR),
                PkgType::Protocol => app_dir.join(TEN_PACKAGES_DIR).join(PROTOCOL_DIR),
                PkgType::System => app_dir.join(TEN_PACKAGES_DIR).join(SYSTEM_DIR),
                PkgType::AddonLoader => app_dir.join(TEN_PACKAGES_DIR).join(ADDON_LOADER_DIR),
                PkgType::App => app_dir.to_path_buf(),
                PkgType::Invalid => {
                    panic!("Should not happen.");
                }
            };

            let bar_clone = bar.clone();
            let progress_counter = progress_counter.clone();
            let pkg_name = format!(
                "{}/{}",
                solver_result.manifest.type_and_name.pkg_type,
                solver_result.manifest.type_and_name.name
            );

            async move {
                // Install the package
                let result =
                    install_pkg_info(tman_config, command_data, solver_result, &base_dir, out)
                        .await;

                // Update progress after installation completes
                if let Some(bar) = bar_clone {
                    let current =
                        progress_counter.fetch_add(1, std::sync::atomic::Ordering::SeqCst) + 1;
                    bar.set_position(current.try_into().unwrap_or(0));
                    bar.set_message(format!("Completed {current}/{total_pkgs}: {pkg_name}"));
                }

                result
            }
        })
        .collect::<Vec<_>>();

    // Execute all installations in parallel.
    let results = futures::future::join_all(install_futures).await;

    // Check for errors.
    for result in results {
        result?;
    }

    if let Some(bar) = &bar {
        bar.finish_with_message("Done");
    }

    Ok(())
}
