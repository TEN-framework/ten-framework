//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod config;
pub mod data;
pub mod package_cache;

use std::path::PathBuf;

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Registry {
    pub index: String,
}

/// Append platform-specific tman directory path to the given base directory.
fn append_tman_path(mut base_dir: PathBuf) -> PathBuf {
    if cfg!(target_os = "windows") {
        base_dir.push("AppData");
        base_dir.push("Roaming");
        base_dir.push("tman");
    } else {
        base_dir.push(".tman");
    }
    base_dir
}

// Determine the tman home directory based on the platform.
pub fn get_home_dir() -> PathBuf {
    // First check if we're in test mode with TEN_MANAGER_HOME_INTERNAL_USE_ONLY
    // set
    if let Ok(test_home) = std::env::var("TEN_MANAGER_HOME_INTERNAL_USE_ONLY") {
        let home_dir = std::path::PathBuf::from(test_home);
        return append_tman_path(home_dir);
    }

    // Normal operation: use system home directory
    let home_dir = dirs::home_dir().expect("Cannot determine home directory.");
    append_tman_path(home_dir)
}
