//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod constants;
pub mod hash;
pub mod language;
pub mod manifest;
pub mod message;
pub mod pkg_basic_info;
pub mod pkg_type;
pub mod pkg_type_and_name;
pub mod predefined_graphs;
pub mod property;
pub mod supports;
pub mod value_type;

use std::{collections::HashMap, path::Path};

use anyhow::{anyhow, Result};
use constants::{
    ADDON_LOADER_DIR, EXTENSION_DIR, MANIFEST_JSON_FILENAME, PROTOCOL_DIR, SYSTEM_DIR,
    TEN_PACKAGES_DIR,
};
use manifest::{
    dependency::ManifestDependency, parse_manifest_from_file, parse_manifest_in_folder, Manifest,
};
use pkg_type::PkgType;
use property::{parse_property_in_folder, Property};
use uuid::Uuid;

use crate::{
    base_dir_pkg_info::PkgsInfoInApp, graph::graph_info::GraphInfo, schema::store::SchemaStore,
};

pub fn localhost() -> &'static str {
    "localhost"
}

#[derive(Clone, Debug)]
pub struct PkgInfo {
    pub manifest: Manifest,
    pub property: Option<Property>,

    pub compatible_score: i32,

    /// This indicates that the package has been installed in the correct
    /// location. For example, in the case of an extension, it means it has
    /// been installed under the `ten_packages/` directory.
    pub is_installed: bool,

    /// A string that represents the location or path where the package is
    /// installed or can be accessed.
    ///
    /// - If `PkgInfo represents a package in the registry, then the `url` field
    ///   represents the download URL that can be used to download the package
    ///   from the registry.
    /// - If PkgInfo represents a package that already exists locally, then the
    ///   url field represents the base directory of the package.
    pub url: String,

    pub hash: String,

    pub schema_store: Option<SchemaStore>,

    /// Indicates that the `pkg_info` represents a local dependency package.
    pub is_local_dependency: bool,
    pub local_dependency_path: Option<String>,
    pub local_dependency_base_dir: Option<String>,
}

impl PkgInfo {
    pub async fn from_metadata(
        url: &str,
        manifest: &Manifest,
        property: &Option<Property>,
        app_base_dir: Option<&str>,
    ) -> Result<Self> {
        let mut pkg_info = PkgInfo {
            manifest: manifest.clone(),
            property: property.clone(),
            compatible_score: 0,

            is_installed: false,
            url: url.to_string(),
            hash: String::new(),

            schema_store: SchemaStore::from_manifest(manifest, app_base_dir).await?,

            is_local_dependency: false,
            local_dependency_path: None,
            local_dependency_base_dir: None,
        };

        pkg_info.hash = pkg_info.gen_hash_hex();

        Ok(pkg_info)
    }

    /// Async serialization method that resolves LocaleContent fields
    pub async fn serialize_manifest_with_resolved_content(&self) -> Result<String> {
        self.manifest.serialize_with_resolved_content().await
    }

    pub fn get_dependency_by_type_and_name(
        &self,
        pkg_type: &str,
        pkg_name: &str,
    ) -> Option<&ManifestDependency> {
        if let Some(dependencies) = &self.manifest.dependencies {
            return dependencies.iter().find(|dep| {
                match dep {
                    ManifestDependency::RegistryDependency {
                        pkg_type: dep_type,
                        name,
                        ..
                    } => dep_type.to_string() == pkg_type && name == pkg_name,
                    ManifestDependency::LocalDependency {
                        ..
                    } => {
                        // For local dependencies, we would need to resolve
                        // the actual type and name by examining the
                        // manifest at the local path, which is beyond the
                        // scope of this method, so we return false.
                        false
                    }
                }
            });
        }
        None
    }
}

/// Retrieves package information from the specified path.
///
/// This function reads and parses the manifest.json and property.json files
/// from the given path, then constructs a PkgInfo object with the parsed data.
pub async fn get_pkg_info_from_path(
    path: &Path,
    is_installed: bool,
    parse_property: bool,
    graphs_cache: &mut Option<&mut HashMap<Uuid, GraphInfo>>,
    app_base_dir: Option<String>,
) -> Result<PkgInfo> {
    let manifest = parse_manifest_in_folder(path).await?;

    let property = if parse_property {
        assert!(graphs_cache.is_some());

        (parse_property_in_folder(
            path,
            graphs_cache.as_mut().unwrap(),
            app_base_dir.clone(),
            Some(manifest.type_and_name.pkg_type),
            Some(manifest.type_and_name.name.clone()),
        )
        .await)?
    } else {
        None
    };

    let mut pkg_info: PkgInfo = PkgInfo::from_metadata(
        path.to_string_lossy().as_ref(),
        &manifest,
        &property,
        app_base_dir.as_deref(),
    )
    .await?;

    pkg_info.is_installed = is_installed;

    // This package comes from a path, not from a registry, so the value of
    // `url` will be the path.
    pkg_info.url = path.to_string_lossy().to_string();

    Ok(pkg_info)
}

/// Collect the corresponding package from the information within the specified
/// path, add it to the collection provided as a parameter, and return the newly
/// collected package.
async fn collect_pkg_info_from_path(
    path: &Path,
    pkgs_info: &mut PkgsInfoInApp,
    parse_property: bool,
    graphs_cache: &mut Option<&mut HashMap<Uuid, GraphInfo>>,
    app_base_dir: Option<String>,
) -> Result<()> {
    let pkg_info =
        get_pkg_info_from_path(path, true, parse_property, graphs_cache, app_base_dir).await?;

    match pkg_info.manifest.type_and_name.pkg_type {
        PkgType::App => {
            pkgs_info.app_pkg_info = Some(pkg_info);
            Ok(())
        }
        PkgType::Extension => {
            if pkgs_info.extension_pkgs_info.is_none() {
                pkgs_info.extension_pkgs_info = Some(Vec::new());
            }
            pkgs_info.extension_pkgs_info.as_mut().unwrap().push(pkg_info);
            Ok(())
        }
        PkgType::Protocol => {
            if pkgs_info.protocol_pkgs_info.is_none() {
                pkgs_info.protocol_pkgs_info = Some(Vec::new());
            }
            pkgs_info.protocol_pkgs_info.as_mut().unwrap().push(pkg_info);
            Ok(())
        }
        PkgType::AddonLoader => {
            if pkgs_info.addon_loader_pkgs_info.is_none() {
                pkgs_info.addon_loader_pkgs_info = Some(Vec::new());
            }
            pkgs_info.addon_loader_pkgs_info.as_mut().unwrap().push(pkg_info);
            Ok(())
        }
        PkgType::System => {
            if pkgs_info.system_pkgs_info.is_none() {
                pkgs_info.system_pkgs_info = Some(Vec::new());
            }
            pkgs_info.system_pkgs_info.as_mut().unwrap().push(pkg_info);
            Ok(())
        }
        _ => Err(anyhow!("Unknown package type")),
    }
}

/// Retrieves information about all installed packages related to a specific
/// application and stores this information in a PkgsInfoInApp struct.
pub async fn get_app_installed_pkgs(
    app_path: &Path,
    parse_property: bool,
    graphs_cache: &mut Option<&mut HashMap<Uuid, GraphInfo>>,
) -> Result<PkgsInfoInApp> {
    let mut pkgs_info = PkgsInfoInApp {
        app_pkg_info: None,
        extension_pkgs_info: None,
        protocol_pkgs_info: None,
        addon_loader_pkgs_info: None,
        system_pkgs_info: None,
    };

    // Process the manifest.json file in the root path.
    collect_pkg_info_from_path(
        app_path,
        &mut pkgs_info,
        parse_property,
        graphs_cache,
        Some(app_path.to_string_lossy().to_string()),
    )
    .await?;

    let app_pkg_info = pkgs_info.app_pkg_info.as_ref().unwrap();
    if app_pkg_info.manifest.type_and_name.pkg_type != PkgType::App {
        return Err(anyhow!("The current working directory does not belong to the `app`."));
    }

    // Define the sub-folders for searching packages.
    let addon_type_dirs = vec![EXTENSION_DIR, PROTOCOL_DIR, ADDON_LOADER_DIR, SYSTEM_DIR];

    for addon_type_dir in addon_type_dirs {
        let addon_type_dir_path = app_path.join(TEN_PACKAGES_DIR).join(addon_type_dir);

        if addon_type_dir_path.exists() && addon_type_dir_path.is_dir() {
            for entry in addon_type_dir_path.read_dir()?.flatten() {
                let path = entry.path();

                if path.is_dir() {
                    // An essential component of a TEN package folder is its
                    // `manifest.json` file that adheres to the TEN standard.
                    let manifest_path = path.join(MANIFEST_JSON_FILENAME);

                    if manifest_path.exists() && manifest_path.is_file() {
                        let manifest = parse_manifest_from_file(&manifest_path).await?;

                        manifest.check_fs_location(
                            Some(addon_type_dir),
                            path.file_name().and_then(|os_str| os_str.to_str()),
                        )?;

                        // This folder contains a manifest.json file and
                        // that manifest.json file is a correct TEN
                        // manifest.json file, so that it means the folder
                        // represents a valid TEN package, and this package is a
                        // _local_ dependency of app.

                        collect_pkg_info_from_path(
                            &path,
                            &mut pkgs_info,
                            parse_property,
                            graphs_cache,
                            Some(app_path.to_string_lossy().to_string()),
                        )
                        .await?;
                    }
                }
            }
        }
    }

    Ok(pkgs_info)
}

/// Identifies local packages that are not tracked in the dependencies list. It
/// iterates over the local packages and checks if each package is present in
/// the dependencies list. If a package is not found in the dependencies list,
/// it is added to the list of untracked local packages.
///
/// # Arguments
/// * `dependencies` - A list of dependencies, required.
/// * `local_pkgs` - A list of local packages, required.
///
/// # Returns
/// A list of untracked local packages.
pub fn find_untracked_local_packages<'a>(
    dependencies: &[&'a PkgInfo],
    local_pkgs: &[&'a PkgInfo],
) -> Vec<&'a PkgInfo> {
    let mut untracked_pkgs = Vec::new();

    for pkg in local_pkgs {
        // Check all dependencies to see if this local package is tracked.
        let is_tracked = dependencies.iter().any(|dep| {
            // Compare type, name, and version
            pkg.manifest.type_and_name.pkg_type == dep.manifest.type_and_name.pkg_type
                && pkg.manifest.type_and_name.name == dep.manifest.type_and_name.name
                && pkg.manifest.version == dep.manifest.version
        });

        if !is_tracked {
            untracked_pkgs.push(*pkg);
        }
    }

    untracked_pkgs
}

/// Return a list of tuples, each tuple contains a local installed package
/// and the package with the same identity but not installed locally in the
/// dependencies list.
///
/// The possible reason could be that the 'supports' is incompatible, or the
/// version calculated by the dependency tree is different.
pub fn find_to_be_replaced_local_pkgs<'a>(
    dependencies: &[&'a PkgInfo],
    local_pkgs: &[&'a PkgInfo],
) -> Vec<(&'a PkgInfo, &'a PkgInfo)> {
    let mut to_be_replaced = Vec::new();

    for dep in dependencies {
        // For each dependency, look for a local package with the same type and
        // name.
        for pkg in local_pkgs {
            // If type and name match but versions differ, this package will be
            // replaced.
            if dep.manifest.type_and_name.pkg_type == pkg.manifest.type_and_name.pkg_type
                && dep.manifest.type_and_name.name == pkg.manifest.type_and_name.name
                && dep.manifest.version != pkg.manifest.version
            {
                to_be_replaced.push((*dep, *pkg));
            }
        }
    }

    to_be_replaced
}

pub fn get_pkg_info_for_extension_addon<'a>(
    pkgs_cache: &'a HashMap<String, PkgsInfoInApp>,
    graph_app_base_dir: &Option<String>,
    app: &Option<String>,
    extension_addon_name: &String,
) -> Option<&'a PkgInfo> {
    let result: Option<&PkgInfo> =
        if let Some((_, pkgs_info_in_app)) = find_pkgs_cache_entry_by_app_uri(pkgs_cache, app) {
            pkgs_info_in_app.extension_pkgs_info.as_ref().and_then(|extension_pkgs_info| {
                extension_pkgs_info.iter().find(|pkg_info| {
                    pkg_info.manifest.type_and_name.pkg_type == PkgType::Extension
                        && pkg_info.manifest.type_and_name.name == *extension_addon_name
                })
            })
        } else {
            None
        };

    if let Some(pkg_info) = result {
        Some(pkg_info)
    } else if let Some(graph_app_base_dir) = graph_app_base_dir {
        pkgs_cache.get(graph_app_base_dir).and_then(|pkgs_info_in_app| {
            pkgs_info_in_app.get_extensions().iter().find(|pkg_info| {
                pkg_info.manifest.type_and_name.pkg_type == PkgType::Extension
                    && pkg_info.manifest.type_and_name.name == *extension_addon_name
            })
        })
    } else {
        None
    }
}

pub fn find_pkgs_cache_entry_by_app_uri<'a>(
    pkgs_cache: &'a HashMap<String, PkgsInfoInApp>,
    app_uri: &Option<String>,
) -> Option<(&'a String, &'a PkgsInfoInApp)> {
    pkgs_cache.iter().find(|(_, pkg_info)| {
        if let Some(app_pkg) = &pkg_info.app_pkg_info {
            if let Some(property) = &app_pkg.property {
                if let Some(ten) = &property.ten {
                    // If the app_uri is None, it means the app is a local app.
                    // In this case, we should return the entry whose app_uri
                    // is None or empty.
                    if app_uri.is_none() {
                        return ten.uri.as_ref().is_none_or(|uri| uri.is_empty());
                    }

                    return ten.uri == *app_uri;
                }
            }
        }
        false
    })
}
