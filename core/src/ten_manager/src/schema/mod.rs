//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod definition;

pub use definition::{DESIGNER_SCHEMA, TMAN_CONFIG_SCHEMA};

use anyhow::Result;
use json5;
use jsonschema::Validator;
use serde_json;

/// Validates a JSON value against the TmanConfig schema.
pub fn validate_tman_config(config_json: &serde_json::Value) -> Result<()> {
    // First parse both schemas into JSON values.
    let mut config_schema: serde_json::Value =
        json5::from_str(TMAN_CONFIG_SCHEMA)?;
    let designer_schema: serde_json::Value = json5::from_str(DESIGNER_SCHEMA)?;

    // Access the designer property in config schema.
    let designer_ref = &mut config_schema["properties"]["designer"];

    // Replace the $ref with the actual designer schema properties.
    if let Some(obj) = designer_ref.as_object_mut() {
        // Remove the $ref key.
        obj.remove("$ref");

        // Copy properties from designer schema.
        if let Some(designer_obj) = designer_schema.as_object() {
            for (key, value) in designer_obj {
                if key != "$schema" && key != "title" {
                    obj.insert(key.clone(), value.clone());
                }
            }
        }
    }

    // Now validate with the combined schema.
    let validator = Validator::new(&config_schema)?;

    if let Err(error) = validator.validate(config_json) {
        return Err(anyhow::anyhow!("Config validation failed: {}", error));
    }

    Ok(())
}

/// Validates a JSON value against the Designer schema.
pub fn validate_designer_config(config_json: &serde_json::Value) -> Result<()> {
    let schema_json: serde_json::Value = json5::from_str(DESIGNER_SCHEMA)?;
    let validator = Validator::new(&schema_json)?;

    if let Err(error) = validator.validate(config_json) {
        return Err(anyhow::anyhow!(
            "Designer config validation failed: {}",
            error
        ));
    }

    Ok(())
}

/// Returns the Designer schema for use in frontend validation.
pub fn get_designer_schema() -> &'static str {
    DESIGNER_SCHEMA
}
