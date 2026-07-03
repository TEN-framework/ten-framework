//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    collections::HashMap,
    fs::OpenOptions,
    io::{BufWriter, Write},
    path::Path,
    str::FromStr,
    sync::Arc,
};

use anyhow::{anyhow, Result};
use console::Emoji;
use semver::Version;
use serde::{Deserialize, Serialize};
use ten_rust::{
    json_schema::validate_manifest_lock_json_string,
    pkg_info::{
        constants::{MANIFEST_JSON_FILENAME, MANIFEST_LOCK_JSON_FILENAME},
        manifest::{
            dependency::{ManifestDependency, TenVersionReq},
            support::ManifestSupport,
            Manifest,
        },
        pkg_basic_info::PkgBasicInfo,
        pkg_type::PkgType,
        pkg_type_and_name::PkgTypeAndName,
        PkgInfo,
    },
    utils::fs::read_file_to_string,
};

use crate::{constants::BUF_WRITER_BUF_SIZE, output::TmanOutput};

// Helper function to check if an Option<Vec> is None or an empty Vec.
fn is_none_or_empty<T>(option: &Option<Vec<T>>) -> bool {
    match option {
        None => true,
        Some(vec) => vec.is_empty(),
    }
}

// The `manifest-lock.json` structure.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestLock {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub version: Option<u32>,

    #[serde(skip_serializing_if = "is_none_or_empty")]
    pub packages: Option<Vec<ManifestLockItem>>,
}

type LockedPkgsInfo<'a> = &'a Vec<&'a PkgInfo>;

impl ManifestLock {
    // Convert a complete `Resolve` to a ManifestLock which can be serialized to
    // a `manifest-lock.json` file.
    pub async fn from_locked_pkgs_info(resolve: LockedPkgsInfo<'_>) -> Result<Self> {
        let mut packages = Vec::new();
        for pkg_info in resolve {
            packages.push(ManifestLockItem::from_pkg_info(pkg_info).await?);
        }

        Ok(ManifestLock {
            version: Some(1), // Not used for now.
            packages: Some(packages),
        })
    }
}

impl FromStr for ManifestLock {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        validate_manifest_lock_json_string(s)?;
        let manifest_lock: ManifestLock = serde_json::from_str(s)?;

        Ok(manifest_lock)
    }
}

impl ManifestLock {
    pub fn get_pkgs(&self) -> HashMap<PkgTypeAndName, PkgInfo> {
        self.packages
            .as_ref()
            .map(|pkgs| {
                pkgs.iter()
                    .map(|pkg| {
                        let pkg_info: PkgInfo = pkg.into();
                        ((&pkg_info).into(), pkg_info)
                    })
                    .collect()
            })
            .unwrap_or_default()
    }

    pub fn print_changes(&self, old_resolve: &ManifestLock, out: Arc<Box<dyn TmanOutput>>) {
        let old_pkgs = old_resolve.get_pkgs();
        let new_pkgs = self.get_pkgs();

        let mut added_pkgs = vec![];
        let mut removed_pkgs = vec![];
        let mut updated_pkgs = vec![];

        for (idt, old_pkg) in old_pkgs.iter() {
            let contains = new_pkgs.contains_key(idt);
            if !contains {
                removed_pkgs.push(old_pkg);
            } else {
                let new_pkg = new_pkgs.get(idt).unwrap();
                if old_pkg.manifest.version != new_pkg.manifest.version {
                    updated_pkgs.push((old_pkg, new_pkg));
                }
            }
        }

        for (idt, new_pkg) in new_pkgs.iter() {
            let contains = old_pkgs.contains_key(idt);
            if !contains {
                added_pkgs.push(new_pkg);
            }
        }

        if !added_pkgs.is_empty() {
            for pkg in added_pkgs.iter() {
                out.normal_line(&format!(
                    "{}  Adding package {} v{}",
                    Emoji("➕", ""),
                    pkg.manifest.type_and_name.name.clone(),
                    pkg.manifest.version
                ));
            }
        }

        if !removed_pkgs.is_empty() {
            for pkg in removed_pkgs.iter() {
                out.normal_line(&format!(
                    "{}  Removing package {} v{}",
                    Emoji("🗑️", ""),
                    pkg.manifest.type_and_name.name.clone(),
                    pkg.manifest.version
                ));
            }
        }

        if !updated_pkgs.is_empty() {
            for (old_pkg, new_pkg) in updated_pkgs.iter() {
                out.normal_line(&format!(
                    "{}  Updating package {} v{} to v{}",
                    Emoji("🔄", ""),
                    old_pkg.manifest.type_and_name.name.clone(),
                    old_pkg.manifest.version,
                    new_pkg.manifest.version
                ));
            }
        }
    }
}

fn are_equal_lockfiles(lock_file_path: &Path, resolve_str: &str) -> bool {
    // Read the contents of the lock file.
    let lock_file_str = read_file_to_string(lock_file_path).unwrap_or_else(|_| "".to_string());

    // Compare the lock file contents with the new resolve string.
    lock_file_str.lines().eq(resolve_str.lines())
}

// Serialize the `ManifestLock` to a JSON string and write it to the lock file.
pub fn write_pkg_lockfile<P: AsRef<Path>>(
    manifest_lock: &ManifestLock,
    app_path: P,
) -> Result<bool> {
    let lock_file_path = app_path.as_ref().join(MANIFEST_LOCK_JSON_FILENAME);

    // For comparison, we still need to serialize to string first
    let encodable_resolve_str = serde_json::to_string_pretty(manifest_lock)?;

    // If the lock file contents haven't changed, we don't need to rewrite it.
    if are_equal_lockfiles(lock_file_path.as_ref(), &encodable_resolve_str) {
        return Ok(false);
    }

    // TODO(xilin): Maybe RWlock is needed.
    // Use BufWriter with custom capacity for improved disk I/O efficiency.
    let file = OpenOptions::new().write(true).create(true).truncate(true).open(&lock_file_path)?;

    let mut buf_writer = BufWriter::with_capacity(BUF_WRITER_BUF_SIZE, file);

    // Serialize directly to the writer to avoid intermediate string allocation
    serde_json::to_writer_pretty(&mut buf_writer, manifest_lock)?;

    buf_writer.flush()?;
    Ok(true)
}

fn parse_manifest_lock_from_file<P: AsRef<Path>>(
    manifest_lock_file_path: P,
) -> Result<ManifestLock> {
    // Read the contents of the manifest-lock.json file.
    let content = read_file_to_string(manifest_lock_file_path)?;

    ManifestLock::from_str(&content)
}

pub fn parse_manifest_lock_in_folder(folder_path: &Path) -> Result<ManifestLock> {
    // Path to the manifest-lock.json file.
    let manifest_lock_file_path = folder_path.join(MANIFEST_LOCK_JSON_FILENAME);

    // Read and parse the manifest-lock.json file.
    let manifest_lock = parse_manifest_lock_from_file(manifest_lock_file_path.clone())?;

    Ok(manifest_lock)
}

// The `dependencies` field structure in `manifest-lock.json`.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestLockItem {
    #[serde(rename = "type")]
    pub pkg_type: String,

    pub name: String,
    pub version: Version,
    pub hash: String,

    #[serde(skip_serializing_if = "is_none_or_empty")]
    pub dependencies: Option<Vec<ManifestLockItemDependencyItem>>,

    #[serde(skip_serializing_if = "is_none_or_empty")]
    pub supports: Option<Vec<ManifestSupport>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub path: Option<String>,
}

impl TryFrom<&ManifestLockItem> for PkgTypeAndName {
    type Error = anyhow::Error;

    fn try_from(manifest: &ManifestLockItem) -> Result<Self> {
        Ok(PkgTypeAndName {
            pkg_type: PkgType::from_str(&manifest.pkg_type).unwrap(),
            name: manifest.name.clone(),
        })
    }
}

impl TryFrom<&ManifestLockItem> for PkgBasicInfo {
    type Error = anyhow::Error;

    fn try_from(manifest: &ManifestLockItem) -> Result<Self> {
        Ok(PkgBasicInfo {
            type_and_name: PkgTypeAndName::try_from(manifest)?,
            version: manifest.version.clone(),
            // If manifest.supports is None, then supports is an empty vector.
            supports: manifest.supports.clone().unwrap_or_default(),
        })
    }
}

async fn get_encodable_deps_from_pkg_deps(
    manifest_deps: Vec<ManifestDependency>,
) -> Result<Vec<ManifestLockItemDependencyItem>> {
    {
        let mut result = Vec::new();

        for dep in manifest_deps {
            let item = match dep {
                ManifestDependency::RegistryDependency {
                    pkg_type,
                    name,
                    ..
                } => ManifestLockItemDependencyItem {
                    pkg_type: pkg_type.to_string(),
                    name,
                },
                ManifestDependency::LocalDependency {
                    path,
                    base_dir,
                    ..
                } => {
                    // For local dependencies, we need to extract info from the
                    // manifest.
                    let base_dir_str = base_dir.as_deref().ok_or_else(|| {
                        anyhow!(
                            "base_dir cannot be None when processing local dependency with path: \
                             {}",
                            path
                        )
                    })?;
                    let abs_path = std::path::Path::new(base_dir_str).join(&path);
                    let dep_manifest_path = abs_path.join(MANIFEST_JSON_FILENAME);

                    let manifest_result =
                        ten_rust::pkg_info::manifest::parse_manifest_from_file(&dep_manifest_path)
                            .await;

                    match manifest_result {
                        Ok(local_manifest) => ManifestLockItemDependencyItem {
                            pkg_type: local_manifest.type_and_name.pkg_type.to_string(),
                            name: local_manifest.type_and_name.name,
                        },
                        Err(_) => {
                            // If we can't parse the manifest, use a
                            // placeholder.
                            ManifestLockItemDependencyItem {
                                pkg_type: "unknown".to_string(),
                                name: path,
                            }
                        }
                    }
                }
            };
            result.push(item);
        }

        Ok(result)
    }
}

impl ManifestLockItem {
    pub async fn from_pkg_info(pkg_info: &PkgInfo) -> Result<Self> {
        let dependencies = match &pkg_info.manifest.dependencies {
            Some(deps) => get_encodable_deps_from_pkg_deps(deps.clone()).await?,
            None => vec![],
        };

        let (pkg_type, name, version, supports) = (
            pkg_info.manifest.type_and_name.pkg_type.to_string(),
            pkg_info.manifest.type_and_name.name.clone(),
            pkg_info.manifest.version.clone(),
            pkg_info.manifest.supports.clone().unwrap_or_default(),
        );

        Ok(Self {
            pkg_type,
            name,
            version,
            hash: pkg_info.hash.to_string(),
            dependencies: if dependencies.is_empty() { None } else { Some(dependencies) },
            supports: if supports.is_empty() { None } else { Some(supports) },
            path: pkg_info.local_dependency_path.clone(),
        })
    }
}

impl<'a> From<&'a ManifestLockItem> for PkgInfo {
    fn from(locked_item: &'a ManifestLockItem) -> Self {
        use ten_rust::pkg_info::{
            manifest::dependency::ManifestDependency, pkg_type_and_name::PkgTypeAndName,
        };

        let dependencies_option = locked_item.clone().dependencies.map(|deps| {
            deps.into_iter()
                .map(|dep| {
                    let pkg_type = match PkgType::from_str(&dep.pkg_type) {
                        Ok(pkg_type) => pkg_type,
                        Err(_) => PkgType::Extension,
                    };
                    ManifestDependency::RegistryDependency {
                        pkg_type,
                        name: dep.name,
                        // Default version requirement.
                        version_req: TenVersionReq::new("*".to_string()).unwrap(),
                    }
                })
                .collect()
        });

        let type_and_name = PkgTypeAndName {
            pkg_type: PkgType::from_str(&locked_item.pkg_type).unwrap_or(PkgType::Extension),
            name: locked_item.name.clone(),
        };

        // Create a manifest with the dependencies.
        let manifest = Manifest {
            type_and_name: type_and_name.clone(),
            version: locked_item.version.clone(),
            description: None,
            display_name: None,
            readme: None,
            dependencies: dependencies_option.clone(),
            dev_dependencies: None,
            tags: None,
            supports: locked_item.supports.clone(),
            api: None,
            package: None,
            scripts: None,
            flattened_api: Arc::new(tokio::sync::RwLock::new(None)),
        };

        PkgInfo {
            compatible_score: 0, // TODO(xilin): default value.
            is_installed: false,
            url: "".to_string(), // TODO(xilin): default value.
            hash: locked_item.hash.clone(),
            manifest,
            property: None,
            schema_store: None,
            is_local_dependency: locked_item.path.is_some(),
            local_dependency_path: locked_item.path.clone(),
            local_dependency_base_dir: None,
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ManifestLockItemDependencyItem {
    #[serde(rename = "type")]
    pub pkg_type: String,

    pub name: String,
}
