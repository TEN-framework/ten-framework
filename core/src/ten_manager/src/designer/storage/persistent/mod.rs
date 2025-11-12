//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod get;
pub mod schema;
pub mod set;

use std::{
    fs::{self, OpenOptions},
    io::{BufWriter, Write},
};

use anyhow::Result;
use serde_json::Value;

use crate::{constants::BUF_WRITER_BUF_SIZE, home::data::get_home_data_path};

/// Read the persistent storage data from disk
pub fn read_persistent_storage() -> Result<Value> {
    let path = get_home_data_path();

    if !path.exists() {
        return Ok(Value::Object(serde_json::Map::new()));
    }

    let content = fs::read_to_string(&path)?;
    let data: Value = serde_json::from_str(&content)?;

    Ok(data)
}

/// Write the persistent storage data to disk
pub fn write_persistent_storage(data: &Value) -> Result<()> {
    let path = get_home_data_path();

    // Ensure the parent directory exists
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let file = OpenOptions::new().write(true).create(true).truncate(true).open(&path)?;

    let mut buf_writer = BufWriter::with_capacity(BUF_WRITER_BUF_SIZE, file);

    serde_json::to_writer_pretty(&mut buf_writer, data)?;
    buf_writer.flush()?;

    Ok(())
}
