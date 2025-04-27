//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use regex::Regex;
use serde::{Deserialize, Deserializer, Serialize};

use crate::pkg_info::value_type::ValueType;

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestApi {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<HashMap<String, ManifestApiPropertyAttributes>>,

    // Only manifest.json of extension has this field.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<Vec<String>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub cmd_in: Option<Vec<ManifestApiMsg>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cmd_out: Option<Vec<ManifestApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub data_in: Option<Vec<ManifestApiMsg>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub data_out: Option<Vec<ManifestApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_frame_in: Option<Vec<ManifestApiMsg>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_frame_out: Option<Vec<ManifestApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_frame_in: Option<Vec<ManifestApiMsg>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_frame_out: Option<Vec<ManifestApiMsg>>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestApiPropertyAttributes {
    #[serde(rename = "type")]
    pub prop_type: ValueType,

    // Used when prop_type is ValueType::Array.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub items: Option<Box<ManifestApiPropertyAttributes>>,

    // Used when prop_type is ValueType::Object.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub properties: Option<HashMap<String, ManifestApiPropertyAttributes>>,

    // Used when prop_type is ValueType::Object.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<Vec<String>>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestApiCmdResult {
    #[serde(default)]
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<HashMap<String, ManifestApiPropertyAttributes>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<Vec<String>>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestApiMsg {
    #[serde(deserialize_with = "validate_msg_name")]
    pub name: String,

    #[serde(default)]
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<HashMap<String, ManifestApiPropertyAttributes>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<Vec<String>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub result: Option<ManifestApiCmdResult>,
}

fn validate_msg_name<'de, D>(deserializer: D) -> Result<String, D::Error>
where
    D: Deserializer<'de>,
{
    let msg_name: String = String::deserialize(deserializer)?;
    let re = Regex::new(r"^[A-Za-z_][A-Za-z0-9_]*$").unwrap();
    if re.is_match(&msg_name) {
        Ok(msg_name)
    } else {
        Err(serde::de::Error::custom(
            "Invalid message name format, it needs to conform to the pattern \
             ^[A-Za-z_][A-Za-z0-9_]*$",
        ))
    }
}
