//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::future::Future;
use std::path::Path;
use std::pin::Pin;

use anyhow::Context;
use semver::VersionReq;
use serde::{Deserialize, Serialize};

use crate::pkg_info::{get_pkg_info_from_path, pkg_type::PkgType, PkgInfo};

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
                ManifestDependency::LocalDependency { path, base_dir } => {
                    // Construct a `PkgInfo` to represent the package
                    // corresponding to the specified path.
                    let base_dir_str = base_dir.as_str();
                    let path_str = path.as_str();

                    let abs_path = Path::new(base_dir_str)
                        .join(path_str)
                        .canonicalize()
                        .with_context(|| {
                            format!(
                                "Failed to canonicalize path: {base_dir_str} \
                                 + {path_str}"
                            )
                        })
                        .ok()?;

                    let pkg_info = get_pkg_info_from_path(
                        &abs_path, false, false, &mut None, None,
                    )
                    .await
                    .ok()?;

                    // Return owned String to avoid referencing local data
                    Some((
                        pkg_info.manifest.type_and_name.pkg_type,
                        pkg_info.manifest.type_and_name.name.clone(),
                    ))
                }
            }
        })
    }
}

impl From<&PkgInfo> for ManifestDependency {
    fn from(pkg_info: &PkgInfo) -> Self {
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
