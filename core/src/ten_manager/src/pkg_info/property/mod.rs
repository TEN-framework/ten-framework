//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fs;
use std::path::Path;

use serde_json::Value;

use ten_rust::pkg_info::constants::PROPERTY_JSON_FILENAME;

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

    // Check for ten.log.file field.
    let log_file = match json_value.get("ten") {
        Some(ten) => match ten.get("log") {
            Some(log) => match log.get("file") {
                Some(file) => file.as_str(),
                None => None,
            },
            None => None,
        },
        None => None,
    };

    // If log file path not found, return None.
    let log_file = log_file?;

    // Check if path is absolute or relative.
    let path = Path::new(log_file);
    if path.is_absolute() {
        // Return absolute path as string.
        Some(log_file.to_string())
    } else {
        // Combine base_dir with relative path.
        let absolute_path = Path::new(base_dir).join(log_file);
        absolute_path.to_str().map(|path_str| path_str.to_string())
    }
}
