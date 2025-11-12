//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use semver::Version;
use serde_json::{json, Value};
use sha2::{Digest, Sha256};

use super::{manifest::support::ManifestSupport, pkg_type::PkgType, PkgInfo};

impl PkgInfo {
    pub fn gen_hash_hex(&self) -> String {
        // Get supports or use empty vector if None
        let supports = self
            .manifest
            .supports
            .as_ref()
            .map_or_else(|| &[] as &[ManifestSupport], |supports| supports.as_slice());

        gen_hash_hex(
            &self.manifest.type_and_name.pkg_type,
            &self.manifest.type_and_name.name,
            &self.manifest.version,
            supports,
        )
    }
}

pub fn gen_hash_hex(
    pkg_type: &PkgType,
    name: &str,
    version: &Version,
    supports: &[ManifestSupport],
) -> String {
    // Create JSON object
    let mut json_obj = json!({
        "type": pkg_type.to_string(),
        "name": name.to_owned(),
        "version": version.to_string(),
    });

    // Add supports field only if it's not empty
    if !supports.is_empty() {
        let supports_array: Vec<Value> = supports
            .iter()
            .map(|support| {
                let mut support_obj = serde_json::Map::new();

                if let Some(os) = &support.os {
                    support_obj.insert("os".to_string(), Value::String(os.to_string()));
                }

                if let Some(arch) = &support.arch {
                    support_obj.insert("arch".to_string(), Value::String(arch.to_string()));
                }

                Value::Object(support_obj)
            })
            .collect();
        json_obj["supports"] = Value::Array(supports_array);
    }

    // Serialize JSON to string
    let json_string =
        serde_json::to_string(&json_obj).expect("Failed to serialize JSON for hash generation");

    // Calculate hash from JSON string
    let mut hasher = Sha256::new();
    hasher.update(json_string);
    let hash_result = hasher.finalize();
    let hash_hex = format!("{hash_result:x}");

    hash_hex
}
