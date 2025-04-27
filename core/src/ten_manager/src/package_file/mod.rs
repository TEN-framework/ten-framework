//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod package;
pub mod unpackage;

use std::path::Path;
use std::sync::Arc;

use anyhow::Result;
use console::Emoji;
use globset::{GlobBuilder, GlobSetBuilder};
use ignore::{overrides::OverrideBuilder, WalkBuilder};

use ten_rust::pkg_info::constants::MANIFEST_JSON_FILENAME;
use ten_rust::pkg_info::PkgInfo;
use ten_rust::pkg_info::{
    constants::TEN_PACKAGES_DIR, manifest::parse_manifest_in_folder,
};

use super::{config::TmanConfig, constants::TEN_PACKAGE_FILE_EXTENSION};
use crate::config::is_verbose;
use crate::output::TmanOutput;
use crate::{constants::DOT_TEN_DIR, fs::pathbuf_to_string_lossy};
use package::tar_gz_files_to_file;

pub fn get_tpkg_file_name(pkg_info: &PkgInfo) -> Result<String> {
    let manifest = &pkg_info.manifest;

    let output_pkg_file_name = format!(
        "{}_{}{}",
        manifest.type_and_name.name,
        manifest.version,
        TEN_PACKAGE_FILE_EXTENSION
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

    let manifest = parse_manifest_in_folder(folder_to_tar_gz)?;

    // Before packing the package, first check if the content of property.json
    // is correct.
    ten_rust::pkg_info::property::check_property_json_of_pkg(
        &folder_to_tar_gz.to_string_lossy(),
    )
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

    let output_tar_gz_file_path_str =
        pathbuf_to_string_lossy(output_pkg_file_path);

    // Collect files to include.
    let mut include_patterns: Option<Vec<String>> = None;
    if let Some(publish) = &manifest.package {
        if let Some(include) = &publish.include {
            include_patterns = Some(vec![]);

            include_patterns.as_mut().unwrap().extend(include.clone());
        }
    }

    let mut globset_builder = GlobSetBuilder::new();

    // manifest.json is needed for all TEN packages.
    globset_builder.add(
        GlobBuilder::new(MANIFEST_JSON_FILENAME)
            .literal_separator(false)
            .build()?,
    );

    if include_patterns.as_ref().is_none() {
        // Include all folders and files by default.
        globset_builder
            .add(GlobBuilder::new("*").literal_separator(false).build()?);
    } else {
        for pattern in &include_patterns.unwrap() {
            globset_builder.add(
                GlobBuilder::new(pattern).literal_separator(true).build()?,
            );
        }
    }
    let include_globset = globset_builder.build()?;

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
    overrides.add(&format!("!/{}/", DOT_TEN_DIR))?;

    // Add exclude pattern for TEN_PACKAGES_DIR.
    overrides.add(&format!("!/{}/", TEN_PACKAGES_DIR))?;

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
            Err(err) => println!("Error: {:?}", err),
        }
    }

    if is_verbose(tman_config.clone()).await {
        out.normal_line("Files to be packed:");
        for file in &files_to_include {
            out.normal_line(&format!("> {:?}", file));
        }
    }

    tar_gz_files_to_file(
        files_to_include,
        folder_to_tar_gz,
        &output_tar_gz_file_path_str,
    )?;

    Ok(output_tar_gz_file_path_str)
}
