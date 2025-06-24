//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::future::Future;
use std::path::Path;
use std::pin::Pin;

use semver::VersionReq;
use serde::{Deserialize, Serialize};

use crate::pkg_info::manifest::parse_manifest_in_folder;
use crate::pkg_info::{pkg_type::PkgType, PkgInfo};

type TypeAndNameFuture<'a> =
    Pin<Box<dyn Future<Output = Option<(PkgType, String)>> + Send + 'a>>;

#[derive(Serialize, Deserialize, Debug, Clone)]
#[serde(untagged)]
pub enum ManifestDependency {
    RegistryDependency {
        #[serde(rename = "type")]
        pkg_type: PkgType,

        name: String,

        #[serde(rename = "version")]
        version_req: VersionReq,
    },

    LocalDependency {
        path: String,

        // Used to record the folder path where the `manifest.json` containing
        // this dependency is located. It is primarily used to parse the `path`
        // field when it contains a relative path.
        #[serde(skip)]
        base_dir: Option<String>,
    },
}

impl ManifestDependency {
    /// Returns the type and name of the dependency if it's a
    /// RegistryDependency. For LocalDependency, it reads the manifest
    /// from the local path to get the type and name.
    pub fn get_type_and_name(&self) -> TypeAndNameFuture<'_> {
        Box::pin(async move {
            match self {
                ManifestDependency::RegistryDependency {
                    pkg_type,
                    name,
                    ..
                } => Some((*pkg_type, name.clone())),
                ManifestDependency::LocalDependency {
                    path, base_dir, ..
                } => {
                    // Construct the full path to the dependency
                    let full_path =
                        Path::new(base_dir.as_deref().unwrap_or_default())
                            .join(path);

                    // Try to canonicalize the path
                    let abs_path = match full_path.canonicalize() {
                        Ok(path) => path,
                        Err(_) => {
                            // If canonicalize fails, we can't read the manifest
                            return None;
                        }
                    };

                    // Read the manifest from the local path
                    match parse_manifest_in_folder(&abs_path).await {
                        Ok(manifest) => Some((
                            manifest.type_and_name.pkg_type,
                            manifest.type_and_name.name,
                        )),
                        Err(_) => {
                            // If we can't read the manifest, return None
                            None
                        }
                    }
                }
            }
        })
    }
}

impl From<&PkgInfo> for ManifestDependency {
    fn from(pkg_info: &PkgInfo) -> Self {
        if pkg_info.is_local_dependency {
            ManifestDependency::LocalDependency {
                path: pkg_info
                    .local_dependency_path
                    .clone()
                    .unwrap_or_default(),
                base_dir: pkg_info
                    .local_dependency_base_dir
                    .clone()
                    .map(|dir| dir.to_string()),
            }
        } else {
            ManifestDependency::RegistryDependency {
                pkg_type: pkg_info.manifest.type_and_name.pkg_type,
                name: pkg_info.manifest.type_and_name.name.clone(),
                version_req: VersionReq::parse(&format!(
                    "{}",
                    pkg_info.manifest.version
                ))
                .unwrap(),
            }
        }
    }
}
