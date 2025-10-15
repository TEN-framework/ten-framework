//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::Result;
use regex::Regex;
use serde::{Deserialize, Deserializer, Serialize};

use crate::pkg_info::{manifest::interface::flatten_manifest_api, value_type::ValueType};

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestApi {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<ManifestApiProperty>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub interface: Option<Vec<ManifestApiInterface>>,

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

impl ManifestApi {
    /// Return the flattened API.
    /// If the api has no interface, return None.
    pub async fn get_flattened_api(
        &mut self,
        base_dir: &str,
        app_base_dir: &str,
    ) -> Result<Option<ManifestApi>> {
        if let Some(interface) = &mut self.interface {
            // Set the base_dir for each interface.
            for interface in interface.iter_mut() {
                interface.base_dir = Some(base_dir.to_string());
            }

            // Flatten the api.
            let mut flattened_api = None;
            flatten_manifest_api(&Some(self.clone()), &mut flattened_api, Some(app_base_dir))
                .await?;
            Ok(flattened_api)
        } else {
            Ok(None)
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct ManifestApiProperty {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub properties: Option<HashMap<String, ManifestApiPropertyAttributes>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<Vec<String>>,
}

impl ManifestApiProperty {
    /// Check if the property is empty (no properties and no required fields)
    pub fn is_empty(&self) -> bool {
        (self.properties.is_none() || self.properties.as_ref().unwrap().is_empty())
            && (self.required.is_none() || self.required.as_ref().unwrap().is_empty())
    }

    /// Get a reference to the properties HashMap, if it exists
    pub fn properties(&self) -> Option<&HashMap<String, ManifestApiPropertyAttributes>> {
        self.properties.as_ref()
    }

    /// Get a mutable reference to the properties HashMap, creating it if it
    /// doesn't exist
    pub fn properties_mut(&mut self) -> &mut HashMap<String, ManifestApiPropertyAttributes> {
        self.properties.get_or_insert_with(HashMap::new)
    }

    /// Create a new empty ManifestApiProperty
    pub fn new() -> Self {
        Self {
            properties: Some(HashMap::new()),
            required: None,
        }
    }
}

impl Default for ManifestApiProperty {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
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

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct ManifestApiCmdResult {
    #[serde(default)]
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<ManifestApiProperty>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct ManifestApiMsg {
    #[serde(deserialize_with = "validate_msg_name")]
    pub name: String,

    #[serde(default)]
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<ManifestApiProperty>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub result: Option<ManifestApiCmdResult>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestApiInterface {
    pub import_uri: String,

    // Used to record the folder path where the `manifest.json` containing this
    // interface is located. It is primarily used to parse the `import_uri`
    // field when it contains a relative path.
    #[serde(skip)]
    pub base_dir: Option<String>,
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
