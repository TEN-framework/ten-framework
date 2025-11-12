//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    collections::HashMap,
    fs::OpenOptions,
    io::{BufReader, BufWriter, Write},
    path::Path,
};

use anyhow::{Context, Result};
use ten_rust::{
    graph::graph_info::GraphInfo,
    pkg_info::{
        constants::{MANIFEST_JSON_FILENAME, PROPERTY_JSON_FILENAME, TEN_FIELD_IN_PROPERTY},
        property::Property,
    },
};
use uuid::Uuid;

use crate::constants::BUF_WRITER_BUF_SIZE;

/// Read json file from disk
fn read_json_file_to_map(path: &str) -> Result<serde_json::Map<String, serde_json::Value>> {
    let property_file =
        OpenOptions::new().read(true).open(path).context("Failed to open property.json file")?;
    let buf_reader = BufReader::new(property_file);
    let property_json: serde_json::Map<String, serde_json::Value> =
        serde_json::from_reader(buf_reader)
            .context("Failed to parse property.json file as JSON object")?;
    Ok(property_json)
}

fn write_json_map_to_file(
    path: &str,
    json: &serde_json::Map<String, serde_json::Value>,
) -> Result<()> {
    let property_file = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .open(path)
        .context("Failed to open property.json file")?;

    let mut buf_writer = BufWriter::with_capacity(BUF_WRITER_BUF_SIZE, property_file);

    serde_json::to_writer_pretty(&mut buf_writer, json)
        .context("Failed to write to property.json file")?;

    buf_writer.flush()?;

    Ok(())
}

/// Write the property json file back to disk.
pub fn write_property_json_file(
    base_dir: &str,
    property_json: &serde_json::Map<String, serde_json::Value>,
) -> Result<()> {
    write_json_map_to_file(
        Path::new(base_dir).join(PROPERTY_JSON_FILENAME).to_str().unwrap(),
        property_json,
    )
}

/// Write the manifest json file back to disk.
pub fn write_manifest_json_file(
    base_dir: &str,
    manifest_json: &serde_json::Map<String, serde_json::Value>,
) -> Result<()> {
    write_json_map_to_file(
        Path::new(base_dir).join(MANIFEST_JSON_FILENAME).to_str().unwrap(),
        manifest_json,
    )
}

/// Patch the property.json file with the given property.
pub fn patch_property_json_file(
    base_dir: &str,
    property: &Property,
    graphs_cache: &HashMap<Uuid, GraphInfo>,
    old_graphs_cache: &HashMap<Uuid, GraphInfo>,
) -> Result<()> {
    // generate patch from the difference between before and after the update of
    // property.ten
    let ten_field_str = TEN_FIELD_IN_PROPERTY.to_string();

    let old_ten_json = serde_json::to_value(
        property
            .property_ten_to_json_map(old_graphs_cache)
            .context("Failed to convert property.ten to JSON map")?,
    )?;

    let new_ten_json = serde_json::to_value(
        property
            .property_ten_to_json_map(graphs_cache)
            .context("Failed to convert property.ten to JSON map")?,
    )?;

    let mut whole_property_json = serde_json::Value::Object(
        // Read from property.json.
        read_json_file_to_map(Path::new(base_dir).join(PROPERTY_JSON_FILENAME).to_str().unwrap())
            .context("Failed to read property.json file")?,
    );

    let mut ten_in_property_json =
        whole_property_json.get(&ten_field_str).cloned().unwrap_or(serde_json::Value::Null);

    // Apply patch to property.json, only "ten" field is updated (there could
    // be other fields added by user at the top level).
    patch_json(&old_ten_json, &new_ten_json, &mut ten_in_property_json)?;

    whole_property_json[ten_field_str] = ten_in_property_json;

    let whole_property_json_map: serde_json::Map<String, serde_json::Value> =
        serde_json::from_value(whole_property_json)
            .context("Failed to convert JSON value to map")?;

    write_property_json_file(base_dir, &whole_property_json_map)?;

    Ok(())
}

/// Patch the json using the difference between the given old and new json.
pub fn patch_json(
    old_json: &serde_json::Value,
    new_json: &serde_json::Value,
    json_to_patch: &mut serde_json::Value,
) -> Result<()> {
    let patch = json_patch::diff(old_json, new_json);
    json_patch::patch(json_to_patch, &patch)?;
    Ok(())
}
