//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod get;
pub mod set;

use anyhow::Result;
use serde_json::Value;
use std::fs;
use std::path::PathBuf;

/// Get the default home directory for tman
fn get_default_home_dir() -> PathBuf {
    // First try to get home directory from environment variable (for testing)
    let mut home_dir = if let Ok(home) = std::env::var("HOME") {
        PathBuf::from(home)
    } else {
        dirs::home_dir().expect("Cannot determine home directory.")
    };

    if cfg!(target_os = "windows") {
        home_dir.push("AppData");
        home_dir.push("Roaming");
        home_dir.push("tman");
    } else {
        home_dir.push(".tman");
    }
    home_dir
}

/// Get the path to the persistent storage data file
pub fn get_persistent_storage_path() -> PathBuf {
    let mut path = get_default_home_dir();
    path.push("data.json");
    path
}

/// Read the persistent storage data from disk
pub fn read_persistent_storage() -> Result<Value> {
    let path = get_persistent_storage_path();

    if !path.exists() {
        return Ok(Value::Object(serde_json::Map::new()));
    }

    let content = fs::read_to_string(&path)?;
    let data: Value = serde_json::from_str(&content)?;
    Ok(data)
}

/// Write the persistent storage data to disk
pub fn write_persistent_storage(data: &Value) -> Result<()> {
    let path = get_persistent_storage_path();

    // Ensure the parent directory exists
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let content = serde_json::to_string_pretty(data)?;
    fs::write(&path, content)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    use std::env;
    use tempfile::TempDir;

    /// Override the default home directory for testing
    fn with_temp_home_dir<F>(f: F)
    where
        F: FnOnce(),
    {
        let temp_dir = TempDir::new().expect("Failed to create temp directory");
        let original_home = env::var("HOME").ok();

        env::set_var("HOME", temp_dir.path());

        f();

        // Restore original HOME
        if let Some(home) = original_home {
            env::set_var("HOME", home);
        } else {
            env::remove_var("HOME");
        }
    }

    #[test]
    fn test_read_nonexistent_storage() {
        with_temp_home_dir(|| {
            let result = read_persistent_storage();
            assert!(result.is_ok());
            let data = result.unwrap();
            assert!(data.is_object());
            assert_eq!(data.as_object().unwrap().len(), 0);
        });
    }

    #[test]
    fn test_write_and_read_storage() {
        with_temp_home_dir(|| {
            let test_data =
                json!({"test_key": "test_value", "nested": {"inner": 42}});

            // Write data
            let write_result = write_persistent_storage(&test_data);
            assert!(write_result.is_ok());

            // Read data
            let read_result = read_persistent_storage();
            assert!(read_result.is_ok());
            let read_data = read_result.unwrap();

            assert_eq!(read_data, test_data);
        });
    }

    #[test]
    fn test_storage_path() {
        with_temp_home_dir(|| {
            let path = get_persistent_storage_path();
            let expected_suffix = if cfg!(target_os = "windows") {
                "AppData/Roaming/tman/data.json"
            } else {
                ".tman/data.json"
            };
            assert!(path.to_string_lossy().ends_with(expected_suffix));
        });
    }
}
