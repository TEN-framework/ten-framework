//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::future::Future;
use std::pin::Pin;

use semver::VersionReq;
use serde::{Deserialize, Serialize};

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
        // TODO(xilin): Make it optional.
        #[serde(skip)]
        base_dir: String,

        // Flattened fields - populated during manifest flattening
        #[serde(skip_serializing_if = "Option::is_none")]
        #[serde(rename = "type")]
        pkg_type: Option<PkgType>,

        #[serde(skip_serializing_if = "Option::is_none")]
        name: Option<String>,

        #[serde(skip_serializing_if = "Option::is_none")]
        #[serde(rename = "version")]
        version_req: Option<VersionReq>,
    },
}

impl ManifestDependency {
    /// Returns the type and name of the dependency if it's a
    /// RegistryDependency. Returns None for LocalDependency as it doesn't
    /// have type and name.
    pub fn get_type_and_name(&self) -> TypeAndNameFuture<'_> {
        Box::pin(async move {
            match self {
                ManifestDependency::RegistryDependency {
                    pkg_type,
                    name,
                    ..
                } => Some((*pkg_type, name.clone())),
                ManifestDependency::LocalDependency {
                    pkg_type, name, ..
                } => {
                    // After flattening, these should always be Some
                    if let (Some(pkg_type), Some(name)) = (pkg_type, name) {
                        Some((*pkg_type, name.clone()))
                    } else {
                        // This should never happen if the manifest was properly
                        // flattened
                        panic!(
                            "LocalDependency not properly flattened: pkg_type \
                             and name must be populated after manifest \
                             flattening"
                        );
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
                    .unwrap_or_default(),
                pkg_type: Some(pkg_info.manifest.type_and_name.pkg_type),
                name: Some(pkg_info.manifest.type_and_name.name.clone()),
                version_req: Some(
                    VersionReq::parse(&format!(
                        "{}",
                        pkg_info.manifest.version
                    ))
                    .unwrap(),
                ),
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
