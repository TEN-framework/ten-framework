//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    collections::HashMap,
    fs,
    path::{Path, PathBuf},
};

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};
use uuid::Uuid;

use super::{
    constants::{PROPERTY_JSON_FILENAME, TEN_STR_PREDEFINED_GRAPHS},
    pkg_type::PkgType,
};
use crate::{
    graph::{graph_info::GraphInfo, is_app_default_loc_or_none},
    json_schema,
    json_schema::ten_validate_property_json_string,
    pkg_info::constants::TEN_FIELD_IN_PROPERTY,
    utils::fs::read_file_to_string,
};

/// Represents the property configuration of a TEN package.
///
/// The property configuration consists of two parts:
/// 1. A special `ten` field that contains TEN-specific configuration.
/// 2. All fields from property.json, stored with preserved order.
///
/// This structure is typically serialized to and deserialized from a JSON file
/// named `property.json` in the package directory. The `ten` field serves as a
/// cache for the "ten" field in property.json for faster access, while
/// `other_fields` stores other fields from property.json
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Property {
    /// TEN-specific configuration properties as a cache for faster access.
    /// This field is not serialized and is only used for internal processing.
    #[serde(skip)]
    pub ten: Option<TenInProperty>,

    // Other fields from property.json, stored with order preserved. This
    // excludes the "ten" field if it exists in property.json.
    #[serde(flatten)]
    pub other_fields: Option<Map<String, Value>>,
}

/// Implements the `FromStr` trait for `Property` to enable parsing from a
/// string.
///
/// This implementation allows creating a `Property` instance from a JSON
/// string, which is useful for loading property configurations from files or
/// string literals. After parsing the JSON, it automatically validates and
/// completes the property configuration to ensure it meets all requirements.
pub async fn parse_property_from_str(
    s: &str,
    graphs_cache: &mut HashMap<Uuid, GraphInfo>,
    app_base_dir: Option<String>,
    belonging_pkg_type: Option<PkgType>,
    belonging_pkg_name: Option<String>,
) -> Result<Property> {
    let mut property: Property = Property {
        ten: None,
        other_fields: None,
    };

    ten_validate_property_json_string(s)?;
    let temp_all_fields: Map<String, Value> = serde_json::from_str(s)?;

    let ten_field_str = TEN_FIELD_IN_PROPERTY.to_string();

    // Extract ten field from temp_all_fields if it exists.
    if let Some(ten_value) = temp_all_fields.get(&ten_field_str) {
        // Process the ten field manually instead of using
        // serde_json::from_value directly. Create a TenInProperty with empty
        // predefined_graphs.
        let mut ten_in_property = TenInProperty {
            predefined_graphs: None,
            uri: None,
        };

        // Get other fields from ten_value using serde.
        if let Value::Object(map) = ten_value {
            // Extract and process predefined_graphs specially.
            if let Some(Value::Array(graphs_array)) = map.get(TEN_STR_PREDEFINED_GRAPHS) {
                let mut graph_infos = Vec::new();

                for graph_value in graphs_array {
                    let graph: GraphInfo = serde_json::from_value(graph_value.clone())?;
                    graph_infos.push(graph);
                }

                ensure_unique_predefined_graph_names(&graph_infos)?;

                let mut temp_graphs_cache = HashMap::new();
                let mut graph_uuids = Vec::new();

                for mut graph_info in graph_infos {
                    graph_info.belonging_pkg_type = belonging_pkg_type;
                    graph_info.belonging_pkg_name = belonging_pkg_name.clone();
                    graph_info.app_base_dir = app_base_dir.clone();

                    graph_info.validate_and_complete_and_flatten().await?;

                    let uuid = Uuid::new_v4();
                    temp_graphs_cache.insert(uuid, graph_info);
                    graph_uuids.push(uuid);
                }

                graphs_cache.extend(temp_graphs_cache);
                ten_in_property.predefined_graphs = Some(graph_uuids);
            }

            // Handle uri if present
            if let Some(uri_value) = map.get("uri") {
                if let Some(uri_str) = uri_value.as_str() {
                    ten_in_property.uri = Some(uri_str.to_string());
                }
            }
        }

        property.ten = Some(ten_in_property);
    }

    // Extract other fields from temp_all_fields.
    let mut other_fields_map = Map::new();

    for (key, value) in temp_all_fields {
        if key == ten_field_str {
            continue;
        }
        other_fields_map.insert(key, value);
    }

    if !other_fields_map.is_empty() {
        property.other_fields = Some(other_fields_map);
    } else {
        property.other_fields = None;
    }

    property.validate_and_complete()?;

    Ok(property)
}

/// Ensures that all predefined graphs have unique names.
fn ensure_unique_predefined_graph_names(graphs: &[GraphInfo]) -> Result<()> {
    // Check for duplicate graph names in a separate scope to limit the
    // lifetime of the HashSet to just this validation step
    let mut seen_graph_names = std::collections::HashSet::new();

    for graph in graphs.iter() {
        if let Some(name) = &graph.name {
            // Note: We're storing references to graph names, which is correct.
            if !seen_graph_names.insert(name) {
                return Err(anyhow::anyhow!(
                    "Duplicate predefined graph name detected: '{}'. Each predefined_graph must \
                     have a unique 'name'.",
                    name
                ));
            }
        }
    }

    Ok(())
}

impl Property {
    /// Validates and completes the property configuration.
    ///
    /// This method ensures that the property configuration is valid and
    /// complete.
    pub fn validate_and_complete(&mut self) -> Result<()> {
        Ok(())
    }

    /// Serializes the property configuration to a JSON file.
    ///
    /// This method doesn't preserve the original order of fields in the
    /// `property.json` file, if there is a original property.json file and you
    /// wish to maintain the original order, please use patch_property_json_file
    /// instead.
    pub fn dump_property_to_file(
        &self,
        property_file_path: &PathBuf,
        graphs_cache: &HashMap<Uuid, GraphInfo>,
    ) -> Result<()> {
        let mut root = serde_json::Map::new();

        if let Some(other_fields) = &self.other_fields {
            for (k, v) in other_fields {
                root.insert(k.clone(), v.clone());
            }
        }

        if self.ten.is_some() {
            let ten_json_map = self.property_ten_to_json_map(graphs_cache)?;
            for (k, v) in ten_json_map {
                root.insert(k, v);
            }
        }

        fs::write(property_file_path, serde_json::to_string_pretty(&root)?)?;

        Ok(())
    }

    /// Convert Property.ten to a complete JSON map, including all graph
    /// information.
    pub fn property_ten_to_json_map(
        &self,
        graphs_cache: &HashMap<Uuid, GraphInfo>,
    ) -> Result<serde_json::Map<String, serde_json::Value>> {
        let mut json_map: serde_json::Map<String, serde_json::Value> = serde_json::Map::new();

        if let Some(ten_in_property) = &self.ten {
            // Handle uri field.
            if let Some(uri) = &ten_in_property.uri {
                json_map.insert("uri".to_string(), serde_json::to_value(uri.clone())?);
            }

            // Handle predefined_graphs field.
            let mut graphs_array = Vec::new();
            if let Some(graph_uuids) = &ten_in_property.predefined_graphs {
                for uuid in graph_uuids {
                    if let Some(graph_info) = graphs_cache.get(uuid) {
                        graphs_array.push(serde_json::to_value(graph_info.clone())?);
                    }
                }
            }
            json_map
                .insert(TEN_STR_PREDEFINED_GRAPHS.to_string(), serde_json::to_value(graphs_array)?);
        }

        Ok(json_map)
    }

    /// Convert Property to a complete JSON map, including the "ten" field and
    /// other fields.
    pub fn property_to_json_map(
        &self,
        graphs_cache: &HashMap<Uuid, GraphInfo>,
    ) -> Result<serde_json::Map<String, serde_json::Value>> {
        let mut json_map: serde_json::Map<String, serde_json::Value> = serde_json::Map::new();

        if let Some(other_fields) = &self.other_fields {
            for (k, v) in other_fields {
                json_map.insert(k.clone(), v.clone());
            }
        }

        if self.ten.is_some() {
            let ten_json_map = self.property_ten_to_json_map(graphs_cache)?;
            json_map.insert(TEN_FIELD_IN_PROPERTY.to_string(), serde_json::to_value(ten_json_map)?);
        }

        Ok(json_map)
    }
}

/// Represents the TEN-specific configuration within a property.json file.
///
/// This structure holds TEN Framework specific settings that are stored in the
/// property.json file of a package.
///
/// # Fields
/// * `predefined_graphs` - Optional list of UUIDs that reference graphs in the
///   graphs_cache. These graphs are predefined by the package.
/// * `uri` - Optional URI for the application. If not specified, defaults to
///   localhost.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct TenInProperty {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub predefined_graphs: Option<Vec<Uuid>>,

    #[serde(skip_serializing_if = "is_app_default_loc_or_none")]
    pub uri: Option<String>,
}

pub fn check_property_json_of_pkg(pkg_dir: &str) -> Result<()> {
    let property_json_path = Path::new(pkg_dir).join(PROPERTY_JSON_FILENAME);
    if !property_json_path.exists() {
        return Ok(());
    }

    json_schema::ten_validate_property_json_file(property_json_path.to_str().unwrap())
}

/// Parses a property.json file into a Property struct.
///
/// This function reads the contents of the specified property file,
/// deserializes it into a Property struct, and validates the structure.
///
/// # Arguments
/// * `property_file_path` - Path to the property.json file.
///
/// # Returns
/// * `Result<Property>` - The parsed and validated Property struct on success,
///   or an error if the file cannot be read or the content is invalid.
async fn parse_property_from_file<P: AsRef<Path>>(
    property_file_path: P,
    graphs_cache: &mut HashMap<Uuid, GraphInfo>,
    app_base_dir: Option<String>,
    belonging_pkg_type: Option<PkgType>,
    belonging_pkg_name: Option<String>,
) -> Result<Option<Property>> {
    if !property_file_path.as_ref().exists() {
        return Ok(None);
    }

    // Validate the property schema only if it is present.
    json_schema::ten_validate_property_json_file(&property_file_path).with_context(|| {
        format!("Failed to validate {}.", property_file_path.as_ref().display())
    })?;

    // Read the contents of the property.json file.
    let content = read_file_to_string(property_file_path)?;

    // Parse the content and validate the property structure.
    let property = parse_property_from_str(
        &content,
        graphs_cache,
        app_base_dir,
        belonging_pkg_type,
        belonging_pkg_name,
    )
    .await?;
    Ok(Some(property))
}

pub async fn parse_property_in_folder(
    folder_path: &Path,
    graphs_cache: &mut HashMap<Uuid, GraphInfo>,
    app_base_dir: Option<String>,
    belonging_pkg_type: Option<PkgType>,
    belonging_pkg_name: Option<String>,
) -> Result<Option<Property>> {
    // Path to the property.json file.
    let property_path = folder_path.join(PROPERTY_JSON_FILENAME);

    // Read and parse the property.json file.
    let property = parse_property_from_file(
        &property_path,
        graphs_cache,
        app_base_dir,
        belonging_pkg_type,
        belonging_pkg_name,
    )
    .await
    .with_context(|| format!("Failed to load {}.", property_path.display()))?;

    Ok(property)
}
