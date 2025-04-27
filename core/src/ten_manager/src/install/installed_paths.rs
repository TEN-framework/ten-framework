//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fs::{self, File},
    io::Write,
    path::Path,
};

use anyhow::Result;
use serde::{Deserialize, Serialize};
use serde_json::json;

use crate::constants::{
    DOT_TEN_DIR, INSTALLED_PATHS_JSON_FILENAME, PACKAGE_INFO_DIR_IN_DOT_TEN_DIR,
};

#[derive(Serialize, Deserialize)]
pub struct InstalledPaths {
    pub paths: Vec<String>,
}

pub fn sort_installed_paths(installed_paths: &mut InstalledPaths) {
    installed_paths.paths.sort_by(|a, b| {
        let a_path = Path::new(a);
        let b_path = Path::new(b);

        let a_depth = a_path.components().count();
        let b_depth = b_path.components().count();

        a_depth.cmp(&b_depth).reverse()
    });
}

pub fn save_installed_paths(
    installed_paths: &InstalledPaths,
    cwd: &Path,
) -> Result<()> {
    let target_dir =
        cwd.join(DOT_TEN_DIR).join(PACKAGE_INFO_DIR_IN_DOT_TEN_DIR);
    let target_file = target_dir.join(INSTALLED_PATHS_JSON_FILENAME);
    fs::create_dir_all(&target_dir)?;

    let json_data = json!(installed_paths.paths);
    let mut file = File::create(target_file)?;
    file.write_all(json_data.to_string().as_bytes())?;

    Ok(())
}
