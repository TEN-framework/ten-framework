//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fmt, future::Future, path::Path, pin::Pin};

use semver::VersionReq;
use serde::{
    de::{Error, Visitor},
    Deserialize, Deserializer, Serialize, Serializer,
};

use crate::pkg_info::{manifest::parse_manifest_in_folder, pkg_type::PkgType, PkgInfo};

type TypeAndNameFuture<'a> = Pin<Box<dyn Future<Output = Option<(PkgType, String)>> + Send + 'a>>;

#[derive(Serialize, Deserialize, Debug, Clone)]
#[serde(untagged)]
pub enum ManifestDependency {
    RegistryDependency {
        #[serde(rename = "type")]
        pkg_type: PkgType,

        name: String,

        #[serde(rename = "version")]
        version_req: TenVersionReq,
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
#[derive(Debug, Clone)]
pub struct TenVersionReq {
    raw_version_req: String,
    processed_version_req: VersionReq,
}

/// write back the raw string of version, make sure its original form is
/// preserved
impl Serialize for TenVersionReq {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.raw_version_req)
    }
}

impl<'de> Deserialize<'de> for TenVersionReq {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct TenVersionVisitor;
        impl<'de> Visitor<'de> for TenVersionVisitor {
            type Value = TenVersionReq;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a version requirement string")
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                let processed = VersionReq::parse(v)
                    .map_err(|err| Error::custom(format!("invalid version req: {err}")))?;
                Ok(TenVersionReq {
                    raw_version_req: v.to_string(),
                    processed_version_req: processed,
                })
            }
        }

        deserializer.deserialize_str(TenVersionVisitor)
    }
}

impl TenVersionReq {
    pub fn new(raw: String) -> Result<Self, semver::Error> {
        let processed = VersionReq::parse(&raw)?;
        Ok(Self {
            raw_version_req: raw,
            processed_version_req: processed,
        })
    }

    pub fn matches(&self, version: &semver::Version) -> bool {
        self.processed_version_req.matches(version)
    }

    pub fn as_raw(&self) -> &str {
        &self.raw_version_req
    }

    pub fn as_processed(&self) -> &VersionReq {
        &self.processed_version_req
    }
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
                    path,
                    base_dir,
                    ..
                } => {
                    // Construct the full path to the dependency
                    let full_path = Path::new(base_dir.as_deref().unwrap_or_default()).join(path);

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
                        Ok(manifest) => {
                            Some((manifest.type_and_name.pkg_type, manifest.type_and_name.name))
                        }
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
                path: pkg_info.local_dependency_path.clone().unwrap_or_default(),
                base_dir: pkg_info.local_dependency_base_dir.clone().map(|dir| dir.to_string()),
            }
        } else {
            let parsed_version_req =
                VersionReq::parse(&pkg_info.manifest.version.clone().to_string()).ok();
            let raw_version = parsed_version_req.clone().unwrap().to_string();

            ManifestDependency::RegistryDependency {
                pkg_type: pkg_info.manifest.type_and_name.pkg_type,
                name: pkg_info.manifest.type_and_name.name.clone(),
                version_req: TenVersionReq {
                    raw_version_req: raw_version,
                    processed_version_req: parsed_version_req.unwrap(),
                },
            }
        }
    }
}
