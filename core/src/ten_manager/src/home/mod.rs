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

// Determine the tman home directory based on the platform.
pub fn get_home_dir() -> PathBuf {
    // First check if we're in test mode with TEN_MANAGER_HOME_INTERNAL_USE_ONLY
    // set
    if let Ok(test_home) = std::env::var("TEN_MANAGER_HOME_INTERNAL_USE_ONLY") {
        let mut home_dir = std::path::PathBuf::from(test_home);
        if cfg!(target_os = "windows") {
            home_dir.push("AppData");
            home_dir.push("Roaming");
            home_dir.push("tman");
        } else {
            home_dir.push(".tman");
        }
        return home_dir;
    }

    // Normal operation: use system home directory
    let mut home_dir =
        dirs::home_dir().expect("Cannot determine home directory.");
    if cfg!(target_os = "windows") {
        home_dir.push("AppData");
        home_dir.push("Roaming");
        home_dir.push("tman");
    } else {
        home_dir.push(".tman");
    }
    home_dir
}
