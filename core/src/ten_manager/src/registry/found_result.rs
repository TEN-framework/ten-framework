//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use ten_rust::pkg_info::{
    manifest::{dependency::ManifestDependency, Manifest},
    pkg_basic_info::PkgBasicInfo,
    PkgInfo,
};

pub const BASIC_SCOPE: [&str; 9] = [
    "type",
    "name",
    "version",
    "supports",
    "dependencies",
    "hash",
    "downloadUrl",
    "contentFormat",
    "tags",
];

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct PkgRegistryInfo {
    #[serde(flatten)]
    pub basic_info: PkgBasicInfo,

    #[serde(with = "dependencies_conversion")]
    #[serde(skip_serializing_if = "Vec::is_empty")]
    #[serde(default)]
    pub dependencies: Vec<ManifestDependency>,

    #[serde(skip_serializing_if = "String::is_empty")]
    #[serde(default)]
    pub hash: String,

    #[serde(rename = "downloadUrl")]
    #[serde(skip_serializing_if = "String::is_empty")]
    #[serde(default)]
    pub download_url: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "contentFormat")]
    pub content_format: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub tags: Option<Vec<String>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<ten_rust::pkg_info::manifest::LocalizedField>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub display_name: Option<ten_rust::pkg_info::manifest::LocalizedField>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub readme: Option<ten_rust::pkg_info::manifest::LocalizedField>,
}

mod dependencies_conversion {
    use serde::{Deserialize, Deserializer, Serialize, Serializer};
    use ten_rust::pkg_info::manifest::dependency::ManifestDependency;

    pub fn serialize<S>(deps: &[ManifestDependency], serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let manifest_deps: Vec<ManifestDependency> = deps.to_vec();
        manifest_deps.serialize(serializer)
    }

    pub fn deserialize<'de, D>(deserializer: D) -> Result<Vec<ManifestDependency>, D::Error>
    where
        D: Deserializer<'de>,
    {
        let manifest_deps: Vec<ManifestDependency> = Deserialize::deserialize(deserializer)?;
        Ok(manifest_deps)
    }
}

pub async fn get_pkg_registry_info_from_manifest(
    download_url: &str,
    manifest: &Manifest,
) -> Result<PkgRegistryInfo> {
    let mut pkg_info = PkgInfo::from_metadata(download_url, manifest, &None, None).await?;

    let mut updated_manifest = pkg_info.manifest.clone();

    // Check and resolve display_name content
    if let Some(ref mut display_name) = updated_manifest.display_name {
        for (_locale, locale_content) in display_name.locales.iter_mut() {
            if locale_content.content.is_none() {
                let content = locale_content
                    .get_content()
                    .await
                    .with_context(|| "Failed to get content for display_name")?;
                locale_content.content = Some(content);
            }
        }
    }

    // Check and resolve description content
    if let Some(ref mut description) = updated_manifest.description {
        for (_locale, locale_content) in description.locales.iter_mut() {
            if locale_content.content.is_none() {
                let content = locale_content
                    .get_content()
                    .await
                    .with_context(|| "Failed to get content for description")?;
                locale_content.content = Some(content);
            }
        }
    }

    // Check and resolve readme content
    if let Some(ref mut readme) = updated_manifest.readme {
        for (_locale, locale_content) in readme.locales.iter_mut() {
            if locale_content.content.is_none() {
                let content = locale_content
                    .get_content()
                    .await
                    .with_context(|| "Failed to get content for readme")?;
                locale_content.content = Some(content);
            }
        }
    }

    // Update the pkg_info with the modified manifest
    pkg_info.manifest = updated_manifest;

    Ok((&pkg_info).into())
}

impl From<&PkgInfo> for PkgRegistryInfo {
    fn from(pkg_info: &PkgInfo) -> Self {
        let dependencies = match &pkg_info.manifest.dependencies {
            Some(deps) => deps.clone(),
            None => vec![],
        };

        PkgRegistryInfo {
            basic_info: PkgBasicInfo::from(pkg_info),
            dependencies,
            hash: pkg_info.hash.clone(),
            download_url: String::new(),
            content_format: None,
            tags: pkg_info.manifest.tags.clone(),
            description: pkg_info.manifest.description.clone(),
            display_name: pkg_info.manifest.display_name.clone(),
            readme: pkg_info.manifest.readme.clone(),
        }
    }
}

impl From<&PkgRegistryInfo> for PkgInfo {
    fn from(pkg_registry_info: &PkgRegistryInfo) -> Self {
        PkgInfo {
            compatible_score: -1,
            is_installed: false,
            url: pkg_registry_info.download_url.clone(),
            hash: pkg_registry_info.hash.clone(),
            manifest: Manifest {
                type_and_name: pkg_registry_info.basic_info.type_and_name.clone(),
                version: pkg_registry_info.basic_info.version.clone(),
                description: pkg_registry_info.description.clone(),
                display_name: pkg_registry_info.display_name.clone(),
                readme: pkg_registry_info.readme.clone(),
                dependencies: Some(pkg_registry_info.dependencies.clone()),
                dev_dependencies: None,
                tags: pkg_registry_info.tags.clone(),
                supports: Some(pkg_registry_info.basic_info.supports.clone()),
                api: None,
                package: None,
                scripts: None,
                flattened_api: Arc::new(tokio::sync::RwLock::new(None)),
            },
            property: None,
            schema_store: None,
            is_local_dependency: false,
            local_dependency_path: None,
            local_dependency_base_dir: None,
        }
    }
}
