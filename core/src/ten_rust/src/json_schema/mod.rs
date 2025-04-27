//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod bindings;
mod definition;

extern crate anyhow;
extern crate jsonschema;
extern crate serde_json;

use std::path::Path;

use anyhow::Result;
use json5;
use jsonschema::Validator;

fn load_schema(content: &str) -> Validator {
    // Use json5 to strip comments from the json string.
    let schema_json: serde_json::Value = json5::from_str(content).unwrap();

    jsonschema::validator_for(&schema_json).unwrap()
}

fn validate_json_object(
    json: &serde_json::Value,
    schema_str: &str,
) -> Result<()> {
    let validator = load_schema(schema_str);

    match validator.validate(json) {
        Ok(()) => Ok(()),
        Err(_) => {
            let mut msgs = String::new();
            for error in validator.iter_errors(json) {
                msgs.push_str(&format!(
                    "{} @ {}\n",
                    error, error.instance_path
                ));
            }
            Err(anyhow::anyhow!("{}", msgs))
        }
    }
}

pub fn ten_validate_manifest_json_string(data: &str) -> Result<()> {
    let manifest_json: serde_json::Value = serde_json::from_str(data)?;
    validate_json_object(&manifest_json, definition::MANIFEST_SCHEMA_DEFINITION)
}

pub fn ten_validate_manifest_json_file(file_path: &str) -> Result<()> {
    let file = std::fs::File::open(file_path)?;
    let reader = std::io::BufReader::new(file);
    let manifest_json: serde_json::Value = serde_json::from_reader(reader)?;

    validate_json_object(&manifest_json, definition::MANIFEST_SCHEMA_DEFINITION)
}

pub fn validate_manifest_lock_json_string(data: &str) -> Result<()> {
    let manifest_lock_json: serde_json::Value = serde_json::from_str(data)?;
    validate_json_object(
        &manifest_lock_json,
        definition::MANIFEST_LOCK_SCHEMA_DEFINITION,
    )
}

pub fn validate_manifest_lock_json_file(file_path: &str) -> Result<()> {
    let file = std::fs::File::open(file_path)?;
    let reader = std::io::BufReader::new(file);
    let manifest_lock_json: serde_json::Value =
        serde_json::from_reader(reader)?;

    validate_json_object(
        &manifest_lock_json,
        definition::MANIFEST_LOCK_SCHEMA_DEFINITION,
    )
}

pub fn ten_validate_property_json_string(data: &str) -> Result<()> {
    let property_json: serde_json::Value = serde_json::from_str(data)?;
    validate_json_object(&property_json, definition::PROPERTY_SCHEMA_DEFINITION)
}

pub fn ten_validate_property_json_file<P: AsRef<Path>>(
    file_path: P,
) -> Result<()> {
    let file = std::fs::File::open(file_path)?;
    let reader = std::io::BufReader::new(file);
    let property_json: serde_json::Value = serde_json::from_reader(reader)?;

    validate_json_object(&property_json, definition::PROPERTY_SCHEMA_DEFINITION)
}
