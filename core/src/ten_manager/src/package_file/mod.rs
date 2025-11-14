//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod package;
pub mod unpackage;

use std::{path::Path, sync::Arc};

use anyhow::Result;
use console::Emoji;
use globset::{GlobBuilder, GlobSetBuilder};
use ignore::{overrides::OverrideBuilder, WalkBuilder};
use package::tar_gz_files_to_file;
use ten_rust::pkg_info::{
    constants::{MANIFEST_JSON_FILENAME, TEN_PACKAGES_DIR},
    manifest::parse_manifest_in_folder,
    PkgInfo,
};

use super::{constants::TEN_PACKAGE_FILE_EXTENSION, home::config::TmanConfig};
use crate::{
    constants::DOT_TEN_DIR, fs::pathbuf_to_string_lossy, home::config::is_verbose,
    output::TmanOutput,
};

pub fn get_tpkg_file_name(pkg_info: &PkgInfo) -> Result<String> {
    let manifest = &pkg_info.manifest;

    let output_pkg_file_name = format!(
        "{}_{}{}",
        manifest.type_and_name.name, manifest.version, TEN_PACKAGE_FILE_EXTENSION
    );

    Ok(output_pkg_file_name)
}

pub async fn create_package_tar_gz_file(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    output_pkg_file_path: &Path,
    folder_to_tar_gz: &Path,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<String> {
    out.normal_line(&format!("{}  Creating package", Emoji("🚚", ":-)")));

    let manifest = parse_manifest_in_folder(folder_to_tar_gz).await?;

    // Before packing the package, first check if the content of property.json
    // is correct.
    ten_rust::pkg_info::property::check_property_json_of_pkg(&folder_to_tar_gz.to_string_lossy())
        .map_err(|e| {
        anyhow::anyhow!(
            "Failed to check property.json for {}:{}, {}",
            manifest.type_and_name.pkg_type,
            manifest.type_and_name.name,
            e
        )
    })?;

    // Generate the package file.
    if output_pkg_file_path.exists() {
        std::fs::remove_file(output_pkg_file_path)?;
    }

    let output_tar_gz_file_path_str = pathbuf_to_string_lossy(output_pkg_file_path);

    // Collect files to include.
    let mut include_patterns: Option<Vec<String>> = None;
    if let Some(publish) = &manifest.package {
        if let Some(include) = &publish.include {
            include_patterns = Some(vec![]);

            include_patterns.as_mut().unwrap().extend(include.clone());
        }
    }

    let mut globset_builder = GlobSetBuilder::new();
    // By default, hidden files and folders are ignored, but if the user
    // explicitly specifies hidden files and folders in the include field, this
    // setting should take effect. Since we will uniformly ignore hidden files
    // and folders in the later code, we search for hidden files and folders
    // explicitly specified by the user here, so they can be added back later.
    // We add hidden files and folders specified in manifest.json to the
    // hidden_globset_builder so they won't be ignored during package creation.
    let mut hidden_globset_builder = GlobSetBuilder::new();

    // manifest.json is needed for all TEN packages.
    globset_builder.add(GlobBuilder::new(MANIFEST_JSON_FILENAME).literal_separator(false).build()?);

    if include_patterns.as_ref().is_none() {
        // Include all folders and files by default.
        globset_builder.add(GlobBuilder::new("*").literal_separator(false).build()?);
    } else {
        for pattern in include_patterns.as_ref().unwrap() {
            // Check if pattern starts with '.' or contains '/.' to identify
            // hidden files/folders
            if pattern.starts_with('.') || pattern.contains("/.") {
                hidden_globset_builder.add(GlobBuilder::new(pattern).build()?);
            } else {
                globset_builder.add(GlobBuilder::new(pattern).literal_separator(true).build()?);
            }
        }
    }
    let include_globset = globset_builder.build()?;
    let hidden_globset = hidden_globset_builder.build()?;

    let mut ignore_builder = WalkBuilder::new(folder_to_tar_gz);

    // Set the default values for the WalkBuilder.

    // Do not consider the file size.
    ignore_builder.max_filesize(None);

    // Do not consider the ignore files from the parent folders.
    ignore_builder.parents(false);

    // Ignore all the hidden files and folders.
    ignore_builder.hidden(true);

    ignore_builder.ignore(false);
    ignore_builder.git_ignore(false);

    let mut overrides = OverrideBuilder::new(folder_to_tar_gz);

    // Add exclude pattern for DOT_TEN_DIR.
    overrides.add(&format!("!/{DOT_TEN_DIR}/"))?;

    // Add exclude pattern for TEN_PACKAGES_DIR, but only if the user didn't
    // explicitly include files from it in the manifest.json.
    let mut should_exclude_ten_packages = true;
    if let Some(ref patterns) = include_patterns {
        for pattern in patterns {
            // Check if the pattern explicitly includes files from ten_packages directory.
            if pattern.starts_with(TEN_PACKAGES_DIR)
                || pattern.starts_with(&format!("{}/", TEN_PACKAGES_DIR))
            {
                should_exclude_ten_packages = false;
                break;
            }
        }
    }

    if should_exclude_ten_packages {
        overrides.add(&format!("!/{TEN_PACKAGES_DIR}/"))?;
    }

    let overrides = overrides.build()?;
    ignore_builder.overrides(overrides);

    let mut files_to_include = vec![];
    for result in ignore_builder.build() {
        match result {
            Ok(entry) => {
                let path = entry.path();
                let name = path.strip_prefix(folder_to_tar_gz)?;

                if include_globset.is_match(name) {
                    files_to_include.push(path.to_path_buf());
                }
            }
            Err(err) => println!("Error: {err:?}"),
        }
    }

    // For hidden files/folders explicitly specified in manifest.json, we will
    // not ignore them when packing the package.
    ignore_builder.hidden(false);
    for result in ignore_builder.build() {
        match result {
            Ok(entry) => {
                let path = entry.path();
                let name = path.strip_prefix(folder_to_tar_gz)?;

                if hidden_globset.is_match(name) {
                    files_to_include.push(path.to_path_buf());
                }
            }
            Err(err) => println!("Error: {err:?}"),
        }
    }

    if is_verbose(tman_config.clone()).await {
        out.normal_line("Files to be packed:");
        for file in &files_to_include {
            out.normal_line(&format!("> {file:?}"));
        }
    }

    tar_gz_files_to_file(files_to_include, folder_to_tar_gz, &output_tar_gz_file_path_str)?;

    Ok(output_tar_gz_file_path_str)
}
