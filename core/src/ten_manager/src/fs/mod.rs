//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod file_type;
pub mod json;
pub mod log_file_watcher;

use std::{
    env,
    path::{Path, PathBuf},
};

use anyhow::{anyhow, Result};
use fs_extra::dir::CopyOptions;
use ten_rust::pkg_info::{constants::MANIFEST_JSON_FILENAME, pkg_type::PkgType};

pub fn copy_folder_recursively(src_dir_path: &String, dest_dir_path: &String) -> Result<()> {
    let mut options = CopyOptions::new();

    // Copy the contents inside the directory.
    options.copy_inside = true;

    fs_extra::dir::copy(src_dir_path, dest_dir_path, &options)
        .map_err(|e| anyhow::anyhow!("Failed to copy directory: {}", e))?;

    Ok(())
}

pub fn get_cwd() -> Result<PathBuf> {
    // Attempt to get the current working directory.
    let cwd = match env::current_dir() {
        Ok(current_path) => current_path,
        // Convert the error to anyhow::Error and return.
        Err(e) => return Err(e.into()),
    };

    // If successful, return the current working directory path.
    Ok(cwd)
}

pub fn pathbuf_to_string_lossy(path_buf: &Path) -> String {
    // Convert the PathBuf to a String, replacing invalid UTF-8 sequences with �
    // (U+FFFD)
    path_buf.to_string_lossy().into_owned()
}

/// Check if the directory specified by `path` is an app directory.
pub async fn check_is_app_folder(path: &Path) -> Result<()> {
    let manifest = ten_rust::pkg_info::manifest::parse_manifest_in_folder(path).await?;
    if manifest.type_and_name.pkg_type != PkgType::App {
        return Err(anyhow!("The `type` in manifest.json is not `app`."));
    }

    Ok(())
}

/// Check if the path is a valid directory.
pub fn check_is_valid_dir(path: &Path) -> Result<()> {
    if !path.exists() {
        return Err(anyhow!("Directory does not exist"));
    }

    if !path.is_dir() {
        return Err(anyhow!("Path is not a directory"));
    }

    Ok(())
}

/// Search upwards for the nearest app directory.
pub async fn find_nearest_app_dir(mut start_dir: PathBuf) -> Result<PathBuf> {
    loop {
        let manifest_path = start_dir.join(MANIFEST_JSON_FILENAME);
        if manifest_path.exists() {
            // If it can be parsed correctly and `type=app`, return it directly.
            let manifest =
                ten_rust::pkg_info::manifest::parse_manifest_in_folder(&start_dir).await?;
            if manifest.type_and_name.pkg_type == PkgType::App {
                return Ok(start_dir);
            }
        }

        if !start_dir.pop() {
            break;
        }
    }

    Err(anyhow!("Cannot find a TEN app directory in current or parent directories."))
}
