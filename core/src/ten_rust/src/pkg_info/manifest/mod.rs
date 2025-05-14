//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod api;
pub mod dependency;
pub mod publish;
pub mod support;

use std::collections::HashMap;
use std::{fmt, fs, path::Path, str::FromStr};

use anyhow::{anyhow, Context, Result};
use once_cell::sync::Lazy;
use regex::Regex;
use semver::Version;
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

use crate::fs::read_file_to_string;
use crate::json_schema::ten_validate_manifest_json_string;
use crate::pkg_info::pkg_type::PkgType;
use crate::{json_schema, pkg_info::constants::MANIFEST_JSON_FILENAME};
use api::ManifestApi;
use dependency::ManifestDependency;
use publish::PackageConfig;
use support::ManifestSupport;

use super::constants::TEN_STR_TAGS;
use super::pkg_type_and_name::PkgTypeAndName;

// Define a structure that mirrors the structure of the JSON file.
#[derive(Debug, Clone)]
pub struct Manifest {
    pub type_and_name: PkgTypeAndName,
    pub version: Version,
    pub dependencies: Option<Vec<ManifestDependency>>,
    pub tags: Option<Vec<String>>,

    // Note: For future extensions, use the 'features' field to describe the
    // functionality of each package.
    pub supports: Option<Vec<ManifestSupport>>,
    pub api: Option<ManifestApi>,
    pub package: Option<PackageConfig>,
    pub scripts: Option<HashMap<String, String>>,

    /// All fields from manifest.json, stored with order preserved.
    pub all_fields: Map<String, Value>,
}

impl Serialize for Manifest {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.all_fields.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for Manifest {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let all_fields = Map::deserialize(deserializer)?;

        // Now extract the fields from all_fields.
        let type_and_name = extract_type_and_name(&all_fields)
            .map_err(serde::de::Error::custom)?;
        let version =
            extract_version(&all_fields).map_err(serde::de::Error::custom)?;
        let dependencies = extract_dependencies(&all_fields)
            .map_err(serde::de::Error::custom)?;
        let tags =
            extract_tags(&all_fields).map_err(serde::de::Error::custom)?;
        let supports =
            extract_supports(&all_fields).map_err(serde::de::Error::custom)?;
        let api = extract_api(&all_fields).map_err(serde::de::Error::custom)?;
        let package =
            extract_package(&all_fields).map_err(serde::de::Error::custom)?;
        let scripts =
            extract_scripts(&all_fields).map_err(serde::de::Error::custom)?;

        Ok(Manifest {
            type_and_name,
            version,
            dependencies,
            tags,
            supports,
            api,
            package,
            scripts,
            all_fields,
        })
    }
}

impl Default for Manifest {
    fn default() -> Self {
        let mut all_fields = Map::new();
        all_fields
            .insert("type".to_string(), Value::String("invalid".to_string()));
        all_fields.insert("name".to_string(), Value::String(String::new()));
        all_fields
            .insert("version".to_string(), Value::String("0.0.0".to_string()));

        Self {
            type_and_name: PkgTypeAndName {
                pkg_type: PkgType::Invalid,
                name: String::new(),
            },
            version: Version::new(0, 0, 0),
            dependencies: None,
            tags: None,
            supports: None,
            api: None,
            package: None,
            scripts: None,
            all_fields,
        }
    }
}

impl FromStr for Manifest {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        ten_validate_manifest_json_string(s)?;

        let value: serde_json::Value = serde_json::from_str(s)?;
        let all_fields = match value {
            Value::Object(map) => map,
            _ => return Err(anyhow!("Expected JSON object")),
        };

        // Extract key fields into the struct fields for easier access.
        let type_and_name = extract_type_and_name(&all_fields)?;
        let version = extract_version(&all_fields)?;
        let dependencies = extract_dependencies(&all_fields)?;
        let tags = extract_tags(&all_fields)?;
        let supports = extract_supports(&all_fields)?;
        let api = extract_api(&all_fields)?;
        let package = extract_package(&all_fields)?;
        let scripts = extract_scripts(&all_fields)?;

        // Create manifest with all fields.
        let manifest = Manifest {
            type_and_name,
            version,
            dependencies,
            tags,
            supports,
            api,
            package,
            scripts,
            all_fields,
        };

        Ok(manifest)
    }
}

fn extract_type_and_name(map: &Map<String, Value>) -> Result<PkgTypeAndName> {
    let pkg_type = if let Some(Value::String(t)) = map.get("type") {
        PkgType::from_str(t)?
    } else {
        return Err(anyhow!("Missing or invalid 'type' field"));
    };

    let name = if let Some(Value::String(n)) = map.get("name") {
        n.clone()
    } else {
        return Err(anyhow!("Missing or invalid 'name' field"));
    };

    Ok(PkgTypeAndName { pkg_type, name })
}

fn extract_version(map: &Map<String, Value>) -> Result<Version> {
    if let Some(Value::String(v)) = map.get("version") {
        Version::parse(v).map_err(|e| anyhow!("Invalid version: {}", e))
    } else {
        Err(anyhow!("Missing or invalid 'version' field"))
    }
}

fn extract_dependencies(
    map: &Map<String, Value>,
) -> Result<Option<Vec<ManifestDependency>>> {
    if let Some(Value::Array(deps)) = map.get("dependencies") {
        let mut result = Vec::new();
        for dep in deps {
            let dep_value = serde_json::from_value(dep.clone())?;
            result.push(dep_value);
        }
        Ok(Some(result))
    } else if map.contains_key("dependencies") {
        Err(anyhow!("'dependencies' field is not an array"))
    } else {
        Ok(None)
    }
}

fn extract_supports(
    map: &Map<String, Value>,
) -> Result<Option<Vec<ManifestSupport>>> {
    if let Some(Value::Array(supports)) = map.get("supports") {
        let mut result = Vec::new();
        for support in supports {
            let support_value = serde_json::from_value(support.clone())?;
            result.push(support_value);
        }
        Ok(Some(result))
    } else if map.contains_key("supports") {
        Err(anyhow!("'supports' field is not an array"))
    } else {
        Ok(None)
    }
}

fn extract_api(map: &Map<String, Value>) -> Result<Option<ManifestApi>> {
    if let Some(api_value) = map.get("api") {
        let api = serde_json::from_value(api_value.clone())?;
        Ok(Some(api))
    } else {
        Ok(None)
    }
}

fn extract_package(map: &Map<String, Value>) -> Result<Option<PackageConfig>> {
    if let Some(package_value) = map.get("package") {
        let package = serde_json::from_value(package_value.clone())?;
        Ok(Some(package))
    } else {
        Ok(None)
    }
}

fn extract_scripts(
    map: &Map<String, Value>,
) -> Result<Option<HashMap<String, String>>> {
    if let Some(Value::Object(scripts_map)) = map.get("scripts") {
        let mut result = HashMap::new();
        for (key, value) in scripts_map {
            if let Value::String(script) = value {
                result.insert(key.clone(), script.clone());
            } else {
                return Err(anyhow!("Script value must be a string"));
            }
        }
        Ok(Some(result))
    } else if map.contains_key("scripts") {
        Err(anyhow!("'scripts' field is not an object"))
    } else {
        Ok(None)
    }
}

fn extract_tags(map: &Map<String, Value>) -> Result<Option<Vec<String>>> {
    // Lazy static initialization of regex that validates the tag format.
    static TAG_REGEX: Lazy<Regex> =
        Lazy::new(|| Regex::new(r"^(ten:)?[A-Za-z_][A-Za-z0-9_]*$").unwrap());

    if let Some(Value::Array(tags)) = map.get(TEN_STR_TAGS) {
        let mut result = Vec::new();
        for tag in tags {
            if let Value::String(tag_str) = tag {
                // Validate tag string format.
                if !TAG_REGEX.is_match(tag_str) {
                    return Err(anyhow!(
                        "Invalid tag format: '{}'. Tags must contain only \
                         alphanumeric characters and underscores, must not \
                         start with a digit, and can only have 'ten:' as \
                         prefix",
                        tag_str
                    ));
                }
                result.push(tag_str.clone());
            } else {
                return Err(anyhow!("Tag value must be a string"));
            }
        }
        Ok(Some(result))
    } else if map.contains_key(TEN_STR_TAGS) {
        Err(anyhow!("'tags' field is not an array"))
    } else {
        Ok(None)
    }
}

impl fmt::Display for Manifest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match serde_json::to_string_pretty(&self.all_fields) {
            Ok(json_str) => write!(f, "{json_str}"),
            Err(_) => write!(f, "Failed to serialize manifest"),
        }
    }
}

impl Manifest {
    pub fn validate_and_complete(&mut self) -> Result<()> {
        Ok(())
    }

    pub fn check_fs_location(
        &self,
        addon_type_folder_name: Option<&str>,
        addon_folder_name: Option<&str>,
    ) -> Result<()> {
        if let Some(addon_folder_name) = addon_folder_name {
            // The package `Foo` must be located inside the folder
            // `Foo/`, so that during runtime dynamic loading, the
            // desired package can be identified simply by searching
            // for the folder name. Additionally, the unique nature
            // of package names can be ensured through the file
            // system's restriction that prevents duplicate folder
            // names within the same directory.
            if self.type_and_name.name != addon_folder_name {
                return Err(anyhow!(format!(
                    "the name of the folder '{}' and the package '{}' are \
                     different",
                    addon_folder_name, self.type_and_name.name
                )));
            }
        }

        if let Some(addon_type_folder_name) = addon_type_folder_name {
            if self.type_and_name.pkg_type.to_string() != addon_type_folder_name
            {
                return Err(anyhow!(format!(
                    "The folder name '{}' does not match the expected package \
                     type '{}'",
                    addon_type_folder_name,
                    self.type_and_name.pkg_type.to_string(),
                )));
            }
        }

        Ok(())
    }
}

pub fn dump_manifest_str_to_file<P: AsRef<Path>>(
    manifest_str: &String,
    manifest_file_path: P,
) -> Result<()> {
    fs::write(manifest_file_path, manifest_str)?;
    Ok(())
}

/// Parses a manifest.json file into a Manifest struct.
///
/// This function reads the contents of the specified manifest file,
/// deserializes it into a Manifest struct, and updates any local dependency
/// paths to use the manifest file's parent directory as the base directory.
pub fn parse_manifest_from_file<P: AsRef<Path>>(
    manifest_file_path: P,
) -> Result<Manifest> {
    // Check if the manifest file exists.
    if !manifest_file_path.as_ref().exists() {
        return Err(anyhow::anyhow!(
            "Manifest file not found at: {}",
            manifest_file_path.as_ref().display()
        ));
    }

    // Validate the manifest schema first.
    // This ensures the file conforms to the TEN manifest schema before
    // attempting to parse it.
    json_schema::ten_validate_manifest_json_file(
        manifest_file_path.as_ref().to_str().ok_or_else(|| {
            anyhow::anyhow!(
                "Failed to convert path to string: {}",
                manifest_file_path.as_ref().display()
            )
        })?,
    )
    .with_context(|| {
        format!("Failed to validate {}.", manifest_file_path.as_ref().display())
    })?;

    // Read the contents of the manifest.json file.
    let content = read_file_to_string(&manifest_file_path)?;

    // Parse the content into a Manifest.
    let mut manifest: Manifest = content.parse()?;

    // Get the parent directory of the manifest file to use as base_dir for
    // local dependencies.
    let manifest_folder_path =
        manifest_file_path.as_ref().parent().ok_or_else(|| {
            anyhow::anyhow!(
                "Failed to determine the parent directory of '{}'",
                manifest_file_path.as_ref().display()
            )
        })?;

    // Update the base_dir for all local dependencies to be the manifest's
    // parent directory.
    if let Some(dependencies) = &mut manifest.dependencies {
        for dep in dependencies.iter_mut() {
            if let ManifestDependency::LocalDependency { base_dir, .. } = dep {
                let base_dir_str = manifest_folder_path
                    .to_str()
                    .ok_or_else(|| {
                        anyhow::anyhow!(
                            "Failed to convert folder path to string"
                        )
                    })?
                    .to_string();

                *base_dir = base_dir_str;
            }
        }
    }

    Ok(manifest)
}

/// Parses a manifest.json file from a specified folder.
///
/// This function locates the manifest.json file in the given folder,
/// validates it against the TEN manifest schema, and then parses it into
/// a Manifest struct. The validation happens before parsing to ensure the
/// file conforms to the expected schema structure.
///
/// # Arguments
/// * `folder_path` - Path to the folder containing the manifest.json file.
///
/// # Returns
/// * `Result<Manifest>` - The parsed and validated Manifest struct on success,
///   or an error if the file cannot be read, parsed, or validated.
pub fn parse_manifest_in_folder(folder_path: &Path) -> Result<Manifest> {
    // Construct the path to the manifest.json file.
    let manifest_path = folder_path.join(MANIFEST_JSON_FILENAME);

    // Read and parse the manifest.json file.
    // This also handles setting the base_dir for local dependencies.
    let manifest =
        parse_manifest_from_file(&manifest_path).with_context(|| {
            format!("Failed to load {}.", manifest_path.display())
        })?;

    Ok(manifest)
}
