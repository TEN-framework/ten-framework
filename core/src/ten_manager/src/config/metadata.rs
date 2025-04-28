//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;
use std::{fs, path::PathBuf};

use anyhow::Result;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::constants::METADATA_FILE;

use super::get_default_home_dir;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct NodeGeometry {
    pub app: Option<String>,
    pub extension: String,

    pub x: i64,
    pub y: i64,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct GraphGeometry {
    pub nodes_geometry: Vec<NodeGeometry>,
}

#[derive(Debug, Serialize, Deserialize, Default, Clone)]
pub struct GraphUiMetadata {
    pub graphs_geometry: HashMap<Uuid, GraphGeometry>,
}

#[derive(Serialize, Deserialize, Debug, Default, Clone)]
pub struct TmanMetadata {
    pub graph_ui: GraphUiMetadata,
}

fn get_default_metadata_path() -> PathBuf {
    let mut config_path = get_default_home_dir();
    config_path.push(METADATA_FILE);
    config_path
}

// Read the internal configuration from the specified path.
pub fn read_metadata() -> Result<Option<TmanMetadata>> {
    let config_path = get_default_metadata_path();
    if !config_path.exists() {
        return Ok(None);
    }

    let config_data = fs::read_to_string(config_path.clone())?;
    let config: TmanMetadata = serde_json::from_str(&config_data)?;

    Ok(Some(config))
}
