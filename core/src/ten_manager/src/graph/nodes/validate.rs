use std::collections::HashMap;

//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp,
    pkg_info::{get_pkg_info_for_extension_addon, PkgInfo},
};

fn validate_extension_property_internal(
    property: &Option<serde_json::Value>,
    extension_pkg_info: &PkgInfo,
) -> Result<(), String> {
    // Found matching extension - check property schema if it exists and request
    // has property.
    if let (Some(prop), Some(schema_store)) =
        (property, &extension_pkg_info.schema_store)
    {
        if let Some(property_schema) = &schema_store.property {
            if let Err(e) = property_schema.validate_json(prop) {
                return Err(format!("Property validation failed: {}", e));
            }
        }
    }

    Ok(())
}

pub fn validate_extension_property(
    property: &Option<serde_json::Value>,
    app: &Option<String>,
    addon: &String,
    graph_app_base_dir: &Option<String>,
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
) -> Result<(), String> {
    if let Some(extension_pkg_info) = get_pkg_info_for_extension_addon(
        pkgs_cache,
        graph_app_base_dir,
        app,
        addon,
    ) {
        // Validate the request payload before proceeding.
        validate_extension_property_internal(property, extension_pkg_info)?
    };

    Ok(())
}
