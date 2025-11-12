//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::path::PathBuf;

use crate::{constants::PACKAGE_CACHE, home::get_home_dir};

pub fn default_enable_package_cache() -> bool {
    true
}

/// Get the default package cache folder, located under the default home
/// directory as `package_cache/`.
pub fn get_default_package_cache_folder() -> PathBuf {
    let mut cache_path = get_home_dir();
    cache_path.push(PACKAGE_CACHE);
    cache_path
}
