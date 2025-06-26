//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod api;
pub mod dependency;
pub mod interface;
pub mod publish;
pub mod support;

use std::collections::{HashMap, HashSet};
use std::sync::Arc;
use std::{fmt, fs, path::Path, str::FromStr};

use anyhow::{anyhow, Context, Result};
use once_cell::sync::Lazy;
use regex::Regex;
use semver::Version;
use serde::{Deserialize, Serialize, Serializer};
use serde_json::{Map, Value};

use crate::json_schema;
use crate::json_schema::ten_validate_manifest_json_string;
use crate::pkg_info::constants::MANIFEST_JSON_FILENAME;
use crate::pkg_info::manifest::interface::flatten_manifest_api;
use crate::pkg_info::pkg_type::PkgType;
use crate::utils::fs::read_file_to_string;
use crate::utils::path::get_real_path_from_import_uri;
use crate::utils::uri::load_content_from_uri;
use api::ManifestApi;
use dependency::ManifestDependency;
use publish::PackageConfig;
use support::ManifestSupport;

use super::constants::TEN_STR_TAGS;
use super::pkg_type_and_name::PkgTypeAndName;

#[derive(Debug, Clone, Deserialize)]
pub struct LocaleContent {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub import_uri: Option<String>,

    // Used to record the folder path where the `manifest.json` containing
    // this LocaleContent is located. It is primarily used to parse the
    // `import_uri` field when it contains a relative path.
    #[serde(skip)]
    pub base_dir: Option<String>,
}

impl Serialize for LocaleContent {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        use serde::ser::SerializeStruct;

        // Try to get content using the async method
        // Since we can't call async method in serialize, we need to handle this
        // differently If content is available, serialize it; otherwise
        // serialize import_uri
        if self.content.is_some() {
            let mut state = serializer.serialize_struct("LocaleContent", 1)?;
            state.serialize_field("content", &self.content)?;
            state.end()
        } else if self.import_uri.is_some() {
            let mut state = serializer.serialize_struct("LocaleContent", 1)?;
            state.serialize_field("import_uri", &self.import_uri)?;
            state.end()
        } else {
            // This should not happen based on validation, but handle it
            // gracefully
            Err(serde::ser::Error::custom(
                "LocaleContent must have either content or import_uri",
            ))
        }
    }
}

impl LocaleContent {
    /// Gets the content of this LocaleContent.
    ///
    /// If the content field is not None, returns it directly.
    /// If the content field is None, loads the content from the import_uri
    /// using the base_dir if needed.
    pub async fn get_content(&self) -> Result<String> {
        // If content is already available, return it directly
        if let Some(content) = &self.content {
            return Ok(content.clone());
        }

        // If content is None, try to load from import_uri
        if let Some(import_uri) = &self.import_uri {
            let real_path = get_real_path_from_import_uri(
                import_uri,
                self.base_dir.as_deref(),
            )?;

            // Load content from URI
            load_content_from_uri(&real_path).await.with_context(|| {
                format!(
                    "Failed to load content from import_uri '{import_uri}' \
                     with base_dir '{:?}'",
                    self.base_dir
                )
            })
        } else {
            // Both content and import_uri are None, this should not happen
            // as it's validated during parsing
            Err(anyhow!(
                "LocaleContent must have either 'content' or 'import_uri'"
            ))
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LocalizedField {
    pub locales: HashMap<String, LocaleContent>,
}

// Define a structure that mirrors the structure of the JSON file.
#[derive(Debug, Clone)]
pub struct Manifest {
    pub type_and_name: PkgTypeAndName,
    pub version: Version,
    pub description: Option<LocalizedField>,
    pub display_name: Option<LocalizedField>,
    pub readme: Option<LocalizedField>,
    pub dependencies: Option<Vec<ManifestDependency>>,
    pub dev_dependencies: Option<Vec<ManifestDependency>>,
    pub tags: Option<Vec<String>>,

    // Note: For future extensions, use the 'features' field to describe the
    // functionality of each package.
    pub supports: Option<Vec<ManifestSupport>>,
    pub api: Option<ManifestApi>,
    pub package: Option<PackageConfig>,
    pub scripts: Option<HashMap<String, String>>,

    /// All fields from manifest.json, stored with order preserved.
    pub all_fields: Map<String, Value>,

    /// The flattened API.
    pub flattened_api: Arc<tokio::sync::RwLock<Option<ManifestApi>>>,
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

        let description = extract_description(&all_fields)
            .map_err(serde::de::Error::custom)?;
        let display_name = extract_display_name(&all_fields)
            .map_err(serde::de::Error::custom)?;
        let readme =
            extract_readme(&all_fields).map_err(serde::de::Error::custom)?;

        let dependencies = extract_dependencies(&all_fields)
            .map_err(serde::de::Error::custom)?;
        let dev_dependencies = extract_dev_dependencies(&all_fields)
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
            description,
            display_name,
            readme,
            dependencies,
            dev_dependencies,
            tags,
            supports,
            api,
            package,
            scripts,
            all_fields,
            flattened_api: Arc::new(tokio::sync::RwLock::new(None)),
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
            description: None,
            display_name: None,
            readme: None,
            dependencies: None,
            dev_dependencies: None,
            tags: None,
            supports: None,
            api: None,
            package: None,
            scripts: None,
            all_fields,
            flattened_api: Arc::new(tokio::sync::RwLock::new(None)),
        }
    }
}

impl Manifest {
    pub fn create_from_str(s: &str) -> Result<Self> {
        ten_validate_manifest_json_string(s)?;

        let value: serde_json::Value = serde_json::from_str(s)?;
        let all_fields = match value {
            Value::Object(map) => map,
            _ => return Err(anyhow!("Expected JSON object")),
        };

        // Extract key fields into the struct fields for easier access.
        let type_and_name = extract_type_and_name(&all_fields)?;
        let version = extract_version(&all_fields)?;

        let description = extract_description(&all_fields)?;
        let display_name = extract_display_name(&all_fields)?;
        let readme = extract_readme(&all_fields)?;
        let dependencies = extract_dependencies(&all_fields)?;
        let dev_dependencies = extract_dev_dependencies(&all_fields)?;

        let tags = extract_tags(&all_fields)?;
        let supports = extract_supports(&all_fields)?;
        let api = extract_api(&all_fields)?;
        let package = extract_package(&all_fields)?;
        let scripts = extract_scripts(&all_fields)?;

        // Create manifest with all fields.
        let manifest = Manifest {
            type_and_name,
            version,
            description,
            display_name,
            readme,
            dependencies,
            dev_dependencies,
            tags,
            supports,
            api,
            package,
            scripts,
            all_fields,
            flattened_api: Arc::new(tokio::sync::RwLock::new(None)),
        };

        Ok(manifest)
    }

    /// Async serialization method that resolves LocaleContent fields
    pub async fn serialize_with_resolved_content(&self) -> Result<String> {
        let mut serialized_fields = self.all_fields.clone();

        // Resolve description field
        if let Some(description) = &self.description {
            let mut resolved_locales = Map::new();
            for (locale, locale_content) in &description.locales {
                let content = locale_content.get_content().await?;
                let mut locale_obj = Map::new();
                locale_obj
                    .insert("content".to_string(), Value::String(content));
                resolved_locales
                    .insert(locale.clone(), Value::Object(locale_obj));
            }
            let mut description_obj = Map::new();
            description_obj
                .insert("locales".to_string(), Value::Object(resolved_locales));
            serialized_fields.insert(
                "description".to_string(),
                Value::Object(description_obj),
            );
        }

        // Resolve display_name field
        if let Some(display_name) = &self.display_name {
            let mut resolved_locales = Map::new();
            for (locale, locale_content) in &display_name.locales {
                let content = locale_content.get_content().await?;
                let mut locale_obj = Map::new();
                locale_obj
                    .insert("content".to_string(), Value::String(content));
                resolved_locales
                    .insert(locale.clone(), Value::Object(locale_obj));
            }
            let mut display_name_obj = Map::new();
            display_name_obj
                .insert("locales".to_string(), Value::Object(resolved_locales));
            serialized_fields.insert(
                "display_name".to_string(),
                Value::Object(display_name_obj),
            );
        }

        // Resolve readme field
        if let Some(readme) = &self.readme {
            let mut resolved_locales = Map::new();
            for (locale, locale_content) in &readme.locales {
                let content = locale_content.get_content().await?;
                let mut locale_obj = Map::new();
                locale_obj
                    .insert("content".to_string(), Value::String(content));
                resolved_locales
                    .insert(locale.clone(), Value::Object(locale_obj));
            }
            let mut readme_obj = Map::new();
            readme_obj
                .insert("locales".to_string(), Value::Object(resolved_locales));
            serialized_fields
                .insert("readme".to_string(), Value::Object(readme_obj));
        }

        serde_json::to_string_pretty(&serialized_fields)
            .context("Failed to serialize manifest with resolved content")
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

/// Generic function to extract LocalizedField from a manifest field
fn extract_localized_field(
    map: &Map<String, Value>,
    field_name: &str,
) -> Result<Option<LocalizedField>> {
    // Lazy static initialization of regex that validates the locale format.
    static LOCALE_REGEX: Lazy<Regex> =
        Lazy::new(|| Regex::new(r"^[a-z]{2}(-[A-Z]{2})?$").unwrap());

    if let Some(Value::Object(field_obj)) = map.get(field_name) {
        if let Some(Value::Object(locales_obj)) = field_obj.get("locales") {
            let mut locales = HashMap::new();
            for (locale, locale_content) in locales_obj {
                // Validate locale string format.
                if !LOCALE_REGEX.is_match(locale) {
                    return Err(anyhow!(
                        "Invalid locale format: '{}'. Locales must be in \
                         format 'xx' or 'xx-YY' (BCP47 format)",
                        locale
                    ));
                }

                if let Value::Object(content_obj) = locale_content {
                    let mut locale_content = LocaleContent {
                        content: None,
                        import_uri: None,
                        base_dir: None,
                    };

                    if let Some(Value::String(content_str)) =
                        content_obj.get("content")
                    {
                        if content_str.is_empty() {
                            return Err(anyhow!(
                                "Content for locale '{}' cannot be empty",
                                locale
                            ));
                        }
                        locale_content.content = Some(content_str.clone());
                    }

                    if let Some(Value::String(import_uri_str)) =
                        content_obj.get("import_uri")
                    {
                        if import_uri_str.is_empty() {
                            return Err(anyhow!(
                                "Import URI for locale '{}' cannot be empty",
                                locale
                            ));
                        }
                        locale_content.import_uri =
                            Some(import_uri_str.clone());
                    }

                    if locale_content.content.is_none()
                        && locale_content.import_uri.is_none()
                    {
                        return Err(anyhow!(
                            "Locale '{}' must have either 'content' or \
                             'import_uri'",
                            locale
                        ));
                    }

                    if locale_content.content.is_some()
                        && locale_content.import_uri.is_some()
                    {
                        return Err(anyhow!(
                            "Locale '{}' cannot have both 'content' and \
                             'import_uri'",
                            locale
                        ));
                    }

                    locales.insert(locale.clone(), locale_content);
                } else {
                    return Err(anyhow!("Locale content must be an object"));
                }
            }

            if locales.is_empty() {
                return Err(anyhow!(
                    "{} locales object cannot be empty",
                    field_name.replace('_', " ")
                ));
            }

            Ok(Some(LocalizedField { locales }))
        } else {
            Err(anyhow!(
                "'{}' field must contain a 'locales' object",
                field_name
            ))
        }
    } else if map.contains_key(field_name) {
        Err(anyhow!("'{}' field is not an object", field_name))
    } else {
        Ok(None)
    }
}

fn extract_description(
    map: &Map<String, Value>,
) -> Result<Option<LocalizedField>> {
    extract_localized_field(map, "description")
}

fn extract_display_name(
    map: &Map<String, Value>,
) -> Result<Option<LocalizedField>> {
    extract_localized_field(map, "display_name")
}

fn extract_readme(map: &Map<String, Value>) -> Result<Option<LocalizedField>> {
    extract_localized_field(map, "readme")
}

fn extract_dependencies(
    map: &Map<String, Value>,
) -> Result<Option<Vec<ManifestDependency>>> {
    if let Some(Value::Array(deps)) = map.get("dependencies") {
        let mut result = Vec::new();
        let mut seen_registry_deps = HashSet::new();

        for dep in deps {
            let dep_value: ManifestDependency =
                serde_json::from_value(dep.clone())?;

            // Check for duplicate registry dependencies (type + name)
            // Only check for registry dependencies, skip local dependencies
            // as they will be checked after flattening
            if let ManifestDependency::RegistryDependency {
                pkg_type,
                name,
                ..
            } = &dep_value
            {
                let key = (*pkg_type, name.clone());
                if seen_registry_deps.contains(&key) {
                    return Err(anyhow!(
                        "Duplicate dependency found: type '{}' and name '{}'",
                        pkg_type,
                        name
                    ));
                }
                seen_registry_deps.insert(key);
            }

            result.push(dep_value);
        }
        Ok(Some(result))
    } else if map.contains_key("dependencies") {
        Err(anyhow!("'dependencies' field is not an array"))
    } else {
        Ok(None)
    }
}

fn extract_dev_dependencies(
    map: &Map<String, Value>,
) -> Result<Option<Vec<ManifestDependency>>> {
    if let Some(Value::Array(deps)) = map.get("dev_dependencies") {
        let mut result = Vec::new();
        let mut seen_registry_deps = HashSet::new();

        for dep in deps {
            let dep_value: ManifestDependency =
                serde_json::from_value(dep.clone())?;

            // Check for duplicate registry dependencies (type + name)
            // Only check for registry dependencies, skip local dependencies
            // as they will be checked after flattening
            if let ManifestDependency::RegistryDependency {
                pkg_type,
                name,
                ..
            } = &dep_value
            {
                let key = (*pkg_type, name.clone());
                if seen_registry_deps.contains(&key) {
                    return Err(anyhow!(
                        "Duplicate dependency found: type '{}' and name '{}'",
                        pkg_type,
                        name
                    ));
                }
                seen_registry_deps.insert(key);
            }

            result.push(dep_value);
        }
        Ok(Some(result))
    } else if map.contains_key("dev_dependencies") {
        Err(anyhow!("'dev_dependencies' field is not an array"))
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

    pub fn get_all_dependencies(&self) -> Vec<&ManifestDependency> {
        // Return all dependencies, including dev dependencies.
        let mut dependencies = Vec::new();
        if let Some(deps) = &self.dependencies {
            dependencies.extend(deps.iter());
        }
        if let Some(dev_deps) = &self.dev_dependencies {
            dependencies.extend(dev_deps.iter());
        }
        dependencies
    }

    pub async fn get_flattened_api(&self) -> Result<Option<ManifestApi>> {
        // If the api contains no interfaces, return api directly.
        if let Some(api) = &self.api {
            if api.interface.is_none()
                || api.interface.as_ref().unwrap().is_empty()
            {
                return Ok(Some(api.clone()));
            } else {
                // If the api contains interfaces, try to flatten it.
                let read_guard = self.flattened_api.read().await;
                if let Some(api) = read_guard.as_ref() {
                    return Ok(Some(api.clone()));
                }
                drop(read_guard);

                let mut write_guard = self.flattened_api.write().await;
                flatten_manifest_api(&self.api, &mut write_guard).await?;

                let flattened = write_guard.as_ref().map(|api| api.clone());
                drop(write_guard);
                return Ok(flattened);
            }
        }

        Ok(None)
    }
}

pub fn dump_manifest_str_to_file<P: AsRef<Path>>(
    manifest_str: &String,
    manifest_file_path: P,
) -> Result<()> {
    fs::write(manifest_file_path, manifest_str)?;
    Ok(())
}

/// Updates the base_dir for all components in the manifest that need it.
///
/// This function sets the base_dir for:
/// - Local dependencies
/// - Display name locale content
/// - Description locale content
/// - Readme locale content
/// - Interface references in the API
///
/// The base_dir is set to the parent directory of the manifest file.
fn update_manifest_base_dirs<P: AsRef<Path>>(
    manifest_file_path: P,
    manifest: &mut Manifest,
) -> Result<()> {
    // Get the parent directory of the manifest file to use as base_dir for
    // local dependencies.
    let manifest_folder_path =
        manifest_file_path.as_ref().parent().ok_or_else(|| {
            anyhow::anyhow!(
                "Failed to determine the parent directory of '{}'",
                manifest_file_path.as_ref().display()
            )
        })?;

    // Convert manifest_folder_path to string once for reuse
    let base_dir_str = manifest_folder_path
        .to_str()
        .ok_or_else(|| {
            anyhow::anyhow!("Failed to convert folder path to string")
        })?
        .to_string();

    // Update the base_dir for all local dependencies to be the manifest's
    // parent directory.
    if let Some(dependencies) = &mut manifest.dependencies {
        for dep in dependencies.iter_mut() {
            if let ManifestDependency::LocalDependency { base_dir, .. } = dep {
                *base_dir = Some(base_dir_str.clone());
            }
        }
    }

    // Update base_dir for display_name
    if let Some(display_name) = &mut manifest.display_name {
        for (_locale, locale_content) in display_name.locales.iter_mut() {
            locale_content.base_dir = Some(base_dir_str.clone());
        }
    }

    // Update base_dir for description
    if let Some(description) = &mut manifest.description {
        for (_locale, locale_content) in description.locales.iter_mut() {
            locale_content.base_dir = Some(base_dir_str.clone());
        }
    }

    // Update base_dir for readme
    if let Some(readme) = &mut manifest.readme {
        for (_locale, locale_content) in readme.locales.iter_mut() {
            locale_content.base_dir = Some(base_dir_str.clone());
        }
    }

    // Update the base_dir for all interface references to be the manifest's
    // parent directory.
    if let Some(api) = &mut manifest.api {
        if let Some(interface) = &mut api.interface {
            for interface in interface.iter_mut() {
                interface.base_dir = Some(base_dir_str.clone());
            }
        }
    }

    Ok(())
}

/// Parses a manifest.json file into a Manifest struct.
///
/// This function reads the contents of the specified manifest file,
/// deserializes it into a Manifest struct, and updates any local dependency
/// paths to use the manifest file's parent directory as the base directory.
pub async fn parse_manifest_from_file<P: AsRef<Path>>(
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
    let mut manifest = Manifest::create_from_str(&content)?;

    // Update all base_dir fields in the manifest.
    update_manifest_base_dirs(&manifest_file_path, &mut manifest)?;

    // Flatten the API.
    {
        let mut flattened_api = manifest.flattened_api.write().await;
        flatten_manifest_api(&manifest.api, &mut flattened_api).await?;
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
pub async fn parse_manifest_in_folder(folder_path: &Path) -> Result<Manifest> {
    // Construct the path to the manifest.json file.
    let manifest_path = folder_path.join(MANIFEST_JSON_FILENAME);

    // Read and parse the manifest.json file.
    // This also handles setting the base_dir for local dependencies.
    let manifest =
        parse_manifest_from_file(&manifest_path).await.with_context(|| {
            format!("Failed to load {}.", manifest_path.display())
        })?;

    Ok(manifest)
}
