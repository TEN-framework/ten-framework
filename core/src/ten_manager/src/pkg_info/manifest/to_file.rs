//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs::File, io::Read, path::Path};

use anyhow::Result;
use serde::de::DeserializeOwned;
use ten_rust::pkg_info::{constants::MANIFEST_JSON_FILENAME, manifest::Manifest};

use crate::fs::json::{patch_json, write_manifest_json_file};

/// Load a JSON file into a deserializable object.
pub fn load_from_file<T: DeserializeOwned>(file_path: &Path) -> Result<T> {
    let mut file = File::open(file_path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;

    let result = serde_json::from_str(&contents)?;
    Ok(result)
}

/// Update the manifest.json file. The original order of entries in the manifest
/// file is preserved.
pub async fn patch_manifest_json_file(pkg_url: &str, manifest: &Manifest) -> Result<()> {
    let new_manifest_str = manifest.serialize_with_resolved_content().await?;
    let new_manifest_json = serde_json::from_str(&new_manifest_str)?;
    let old_manifest =
        load_from_file::<Manifest>(Path::new(pkg_url).join(MANIFEST_JSON_FILENAME).as_path())?;
    let old_manifest_str = old_manifest.serialize_with_resolved_content().await?;
    let old_manifest_json = serde_json::from_str(&old_manifest_str)?;
    let mut manifest_json = serde_json::from_str(&old_manifest_str)?;

    patch_json(&old_manifest_json, &new_manifest_json, &mut manifest_json)?;

    write_manifest_json_file(pkg_url, manifest_json.as_object().unwrap())
}
