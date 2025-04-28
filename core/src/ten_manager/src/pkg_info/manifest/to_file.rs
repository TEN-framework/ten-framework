//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fs::File;
use std::io::Read;
use std::path::Path;

use anyhow::Result;
use serde::de::DeserializeOwned;
use serde_json::Value;

use ten_rust::pkg_info::manifest::dependency::ManifestDependency;

use crate::json::write_manifest_json_file;

/// Load a JSON file into a deserializable object.
pub fn load_from_file<T: DeserializeOwned>(file_path: &Path) -> Result<T> {
    let mut file = File::open(file_path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;

    let result = serde_json::from_str(&contents)?;
    Ok(result)
}

/// Update the manifest file, handling both adding new dependencies and removing
/// existing ones.
///
/// Both `deps_to_add` and `deps_to_remove` are optional, allowing for:
///   - Adding new dependencies without removing any.
///   - Removing dependencies without adding any new ones.
///   - Both adding and removing dependencies in a single call.
///
/// In both cases, the original order of entries in the manifest file is
/// preserved.
pub fn update_manifest_all_fields(
    pkg_url: &str,
    manifest_all_fields: &mut serde_json::Map<String, Value>,
    deps_to_add: Option<&Vec<ManifestDependency>>,
    deps_to_remove: Option<&Vec<ManifestDependency>>,
) -> Result<()> {
    // Process dependencies array.
    if let Some(Value::Array(dependencies_array)) =
        manifest_all_fields.get_mut("dependencies")
    {
        // Remove dependencies if requested.
        if let Some(remove_deps) = deps_to_remove {
            if !remove_deps.is_empty() {
                // First, convert dependencies to remove into comparable form.
                let deps_to_remove_serialized: Vec<String> = remove_deps
                    .iter()
                    .filter_map(|dep| {
                        let dep_value = serde_json::to_value(dep).ok()?;
                        serde_json::to_string(&dep_value).ok()
                    })
                    .collect();

                // Filter out dependencies to remove.
                dependencies_array.retain(|item| {
                    if let Ok(item_str) = serde_json::to_string(item) {
                        !deps_to_remove_serialized.contains(&item_str)
                    } else {
                        true // Keep items that can't be serialized.
                    }
                });
            }
        }

        // Add new dependencies if provided.
        if let Some(new_deps) = deps_to_add {
            if !new_deps.is_empty() {
                // Append new dependencies to the existing array.
                for new_dep in new_deps {
                    if let Ok(new_dep_value) = serde_json::to_value(new_dep) {
                        dependencies_array.push(new_dep_value);
                    }
                }
            }
        }
    } else if let Some(new_deps) = deps_to_add {
        // No dependencies array in all_fields yet, create a new one if we have
        // new dependencies.
        if !new_deps.is_empty() {
            let mut dependencies_array = Vec::new();

            // Add all new dependencies to the array.
            for new_dep in new_deps {
                if let Ok(new_dep_value) = serde_json::to_value(new_dep) {
                    dependencies_array.push(new_dep_value);
                }
            }

            manifest_all_fields.insert(
                "dependencies".to_string(),
                Value::Array(dependencies_array),
            );
        }
    }

    write_manifest_json_file(pkg_url, manifest_all_fields)
}
