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

use ten_rust::pkg_info::manifest::Manifest;

use crate::fs::json::{patch_json, write_manifest_json_file};
use ten_rust::pkg_info::constants::MANIFEST_JSON_FILENAME;

/// Load a JSON file into a deserializable object.
pub fn load_from_file<T: DeserializeOwned>(file_path: &Path) -> Result<T> {
    let mut file = File::open(file_path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;

    let result = serde_json::from_str(&contents)?;
    Ok(result)
}

/// Update the manifest.json file. The original order of entries in the manifest file
/// is preserved.
pub async fn patch_manifest_json_file(
    pkg_url: &str,
    manifest: &Manifest,
) -> Result<()> {
    let new_manifest_str = manifest.serialize_with_resolved_content().await?;
    let new_manifest_json = serde_json::from_str(&new_manifest_str)?;
    let old_manifest = load_from_file::<Manifest>(
        Path::new(pkg_url).join(MANIFEST_JSON_FILENAME).as_path())?;
    let old_manifest_str = old_manifest.serialize_with_resolved_content().await?;
    let old_manifest_json = serde_json::from_str(&old_manifest_str)?;
    let mut manifest_json = serde_json::from_str(&old_manifest_str)?;

    patch_json(&old_manifest_json, &new_manifest_json, &mut manifest_json)?;

     // Read original content of manifest.json file
    let raw_manifest_str = std::fs::read_to_string(
        Path::new(pkg_url).join(MANIFEST_JSON_FILENAME)
    )?;
    let raw_manifest: serde_json::Value = serde_json::from_str(&raw_manifest_str)?;

    // After patch, restore the version field into its original form (e.g. 1.0.0)
    // instead of caret version (e.g. ^1.0.0).
    if let Some(deps) = manifest_json.get_mut("dependencies").and_then(|v| v.as_array_mut()) {
        if let Some(raw_deps) = raw_manifest.get("dependencies").and_then(|d| d.as_array()) {
            for dep in deps {
                if let Some(current_version) = dep.get("version").and_then(|v| v.as_str()) {
                    let current_name = dep.get("name").and_then(|n| n.as_str()).unwrap_or("");
                    let current_type = dep.get("type").and_then(|t| t.as_str()).unwrap_or("");
                    // find corresponding dependency in original manifest
                    for raw_dep in raw_deps {
                        let raw_name = raw_dep.get("name").and_then(|n| n.as_str()).unwrap_or("");
                        let raw_type = raw_dep.get("type").and_then(|t| t.as_str()).unwrap_or("");
                        if raw_name == current_name && raw_type == current_type {
                            let raw_version = raw_dep.get("version").and_then(|v| v.as_str()).unwrap_or("");
                            let current_version_clean = current_version.trim_start_matches('^');
                            if raw_version == current_version_clean
                                && !raw_version.starts_with('^')
                                && current_version.starts_with('^')
                            {
                                *dep.get_mut("version").unwrap() = serde_json::Value::String(raw_version.to_string());
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    if let Some(deps) = manifest_json.get_mut("dev_dependencies").and_then(|v| v.as_array_mut()) {
        if let Some(raw_deps) = raw_manifest.get("dev_dependencies").and_then(|d| d.as_array()) {
            for dep in deps {
                if let Some(current_version) = dep.get("version").and_then(|v| v.as_str()) {
                    let current_name = dep.get("name").and_then(|n| n.as_str()).unwrap_or("");
                    let current_type = dep.get("type").and_then(|t| t.as_str()).unwrap_or("");
                     // find corresponding dependency in original manifest
                    for raw_dep in raw_deps {
                        let raw_name = raw_dep.get("name").and_then(|n| n.as_str()).unwrap_or("");
                        let raw_type = raw_dep.get("type").and_then(|t| t.as_str()).unwrap_or("");
                        if raw_name == current_name && raw_type == current_type {
                            let raw_version = raw_dep.get("version").and_then(|v| v.as_str()).unwrap_or("");
                            let current_version_clean = current_version.trim_start_matches('^');
                            if raw_version == current_version_clean
                                && !raw_version.starts_with('^')
                                && current_version.starts_with('^')
                            {
                                *dep.get_mut("version").unwrap() = serde_json::Value::String(raw_version.to_string());
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    write_manifest_json_file(pkg_url, manifest_json.as_object().unwrap())
}
