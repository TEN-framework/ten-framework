//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::path::PathBuf;

use crate::{constants::DATA_JSON, home::get_home_dir};

/// Get the path to the persistent storage data file
pub fn get_home_data_path() -> PathBuf {
    let mut path = get_home_dir();
    path.push(DATA_JSON);
    path
}
