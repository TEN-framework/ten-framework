//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs, path::Path};

use serde_json::Value;
use ten_rust::{
    log::{AdvancedLogConfig, AdvancedLogEmitter},
    pkg_info::constants::PROPERTY_JSON_FILENAME,
};

/// Get the log file path from property.json if it exists.
pub fn get_log_file_path(base_dir: &str) -> Option<String> {
    // Create path to property.json
    let property_file_path = Path::new(base_dir).join(PROPERTY_JSON_FILENAME);

    // Check if property.json exists.
    if !property_file_path.exists() {
        return None;
    }

    // Read property.json file.
    let file_content = match fs::read_to_string(&property_file_path) {
        Ok(content) => content,
        Err(_) => return None,
    };

    // Parse JSON.
    let json_value: Value = match serde_json::from_str(&file_content) {
        Ok(value) => value,
        Err(_) => return None,
    };

    // Parse log.
    let log = json_value.get("ten")?.get("log")?;
    let log_config: AdvancedLogConfig = serde_json::from_value(log.clone()).unwrap();

    // Check for ten.log.file field.
    let log_file = match log_config.handlers.first().unwrap().emitter.clone() {
        AdvancedLogEmitter::File(config) => config.path,
        _ => return None,
    };

    // Check if path is absolute or relative.
    let path = Path::new(&log_file);
    if path.is_absolute() {
        // Return absolute path as string.
        Some(log_file.to_string())
    } else {
        // Combine base_dir with relative path.
        let absolute_path = Path::new(base_dir).join(log_file);
        absolute_path.to_str().map(|path_str| path_str.to_string())
    }
}
