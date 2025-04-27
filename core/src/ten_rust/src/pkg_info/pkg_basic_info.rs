//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    cmp::Ordering,
    hash::{Hash, Hasher},
};

use anyhow::Result;
use semver::Version;
use serde::{Deserialize, Serialize};

use super::{
    manifest::support::ManifestSupport, manifest::Manifest,
    pkg_type_and_name::PkgTypeAndName, PkgInfo,
};

// Basic info refers to the information used to "uniquely" identify a TEN
// package. It includes the fields: type, name, version, and supports, which
// together can be thought of as a unique ID representing a specific TEN
// package.
#[derive(Clone, Debug, Eq, Serialize, Deserialize)]
pub struct PkgBasicInfo {
    #[serde(flatten)]
    pub type_and_name: PkgTypeAndName,

    pub version: Version,

    // Since the declaration 'does not support all environments' has no
    // practical meaning, not specifying the 'supports' field or specifying an
    // empty 'supports' field both represent support for all environments.
    // Therefore, the 'supports' field here is not an option. An empty
    // 'supports' field represents support for all combinations of
    // environments.
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub supports: Vec<ManifestSupport>,
}

impl Hash for PkgBasicInfo {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.type_and_name.hash(state);
        self.version.hash(state);
        self.supports.hash(state);
    }
}

impl PartialEq for PkgBasicInfo {
    fn eq(&self, other: &Self) -> bool {
        self.type_and_name.pkg_type == other.type_and_name.pkg_type
            && self.type_and_name.name == other.type_and_name.name
            && self.version == other.version
            && self.supports == other.supports
    }
}

impl From<&PkgInfo> for PkgBasicInfo {
    fn from(pkg_info: &PkgInfo) -> Self {
        PkgBasicInfo {
            type_and_name: pkg_info.manifest.type_and_name.clone(),
            version: pkg_info.manifest.version.clone(),
            supports: match &pkg_info.manifest.supports {
                Some(supports) => supports.clone(),
                None => Vec::new(),
            },
        }
    }
}

impl PartialOrd for PkgBasicInfo {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for PkgBasicInfo {
    fn cmp(&self, other: &Self) -> Ordering {
        // Compare pkg_type.
        if self.type_and_name.pkg_type != other.type_and_name.pkg_type {
            return self
                .type_and_name
                .pkg_type
                .cmp(&other.type_and_name.pkg_type);
        }

        // Compare name.
        if self.type_and_name.name != other.type_and_name.name {
            return self.type_and_name.name.cmp(&other.type_and_name.name);
        }

        // Compare version.
        if self.version != other.version {
            return self.version.cmp(&other.version);
        }

        // Compare supports.
        self.supports.cmp(&other.supports)
    }
}

impl TryFrom<&Manifest> for PkgBasicInfo {
    type Error = anyhow::Error;

    fn try_from(manifest: &Manifest) -> Result<Self> {
        Ok(PkgBasicInfo {
            type_and_name: PkgTypeAndName::from(manifest),
            version: manifest.version.clone(),
            supports: match &manifest.supports {
                Some(supports) => supports.clone(),
                None => Vec::new(),
            },
        })
    }
}
