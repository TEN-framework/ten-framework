//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs::OpenOptions, path::Path};

use anyhow::{Context, Result};
use std::io::{BufWriter, Write};
use ten_rust::pkg_info::constants::{
    MANIFEST_JSON_FILENAME, PROPERTY_JSON_FILENAME,
};

use crate::constants::BUF_WRITER_BUF_SIZE;

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

    let mut buf_writer =
        BufWriter::with_capacity(BUF_WRITER_BUF_SIZE, property_file);

    // Serialize the property_all_fields map directly to preserve field order.
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
