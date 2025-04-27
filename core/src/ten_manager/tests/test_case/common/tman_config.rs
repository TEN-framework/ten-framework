//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs, path::PathBuf};

/// Finds the `config.json` by:
/// 1. Starting from current directory
/// 2. Looking up parent directories until it finds one with `out/` folder.
/// 3. Finding the only subfolder in `out/`.
/// 4. Finding the only subfolder within that subfolder.
/// 5. Locating the `config.json` at `tests/local_registry/config.json` in that
///    directory.
pub fn find_config_json() -> Option<PathBuf> {
    let mut current_dir = std::env::current_dir().ok()?;

    // Keep going up parent directories until we find `out/` folder.
    loop {
        let out_dir = current_dir.join("out");
        if out_dir.exists() && out_dir.is_dir() {
            // Find the only subfolder in `out/`.
            let out_subfolders = fs::read_dir(&out_dir)
                .ok()?
                .filter_map(|entry| {
                    let entry = entry.ok()?;
                    let path = entry.path();
                    if path.is_dir() {
                        Some(path)
                    } else {
                        None
                    }
                })
                .collect::<Vec<_>>();

            if out_subfolders.len() != 1 {
                return None;
            }

            let first_subfolder = &out_subfolders[0];

            // Find the only subfolder in the first subfolder.
            let nested_subfolders = fs::read_dir(first_subfolder)
                .ok()?
                .filter_map(|entry| {
                    let entry = entry.ok()?;
                    let path = entry.path();
                    if path.is_dir() {
                        Some(path)
                    } else {
                        None
                    }
                })
                .collect::<Vec<_>>();

            if nested_subfolders.len() != 1 {
                return None;
            }

            // Look for `config.json` at `tests/local_registry/config.json`.
            let config_path = nested_subfolders[0]
                .join("tests")
                .join("local_registry")
                .join("config.json");

            if config_path.exists() {
                return Some(config_path);
            }

            return None;
        }

        // Go up to parent directory.
        if let Some(parent) = current_dir.parent() {
            current_dir = parent.to_path_buf();
        } else {
            // Reached root directory, `config.json` not found.
            return None;
        }
    }
}
