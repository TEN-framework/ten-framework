//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::hash::{Hash, Hasher};

use serde::{Deserialize, Serialize};

use super::{pkg_basic_info::PkgBasicInfo, pkg_type::PkgType, PkgInfo};
use crate::pkg_info::manifest::Manifest;

#[derive(Clone, Debug, Serialize, Deserialize, PartialOrd, Eq)]
pub struct PkgTypeAndName {
    #[serde(rename = "type")]
    pub pkg_type: PkgType,

    pub name: String,
}

impl Default for PkgTypeAndName {
    fn default() -> Self {
        Self {
            pkg_type: PkgType::Invalid,
            name: String::new(),
        }
    }
}

impl Hash for PkgTypeAndName {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.pkg_type.hash(state);
        self.name.hash(state);
    }
}

impl PartialEq for PkgTypeAndName {
    fn eq(&self, other: &Self) -> bool {
        self.pkg_type == other.pkg_type && self.name == other.name
    }
}

impl From<&Manifest> for PkgTypeAndName {
    fn from(manifest: &Manifest) -> Self {
        manifest.type_and_name.clone()
    }
}

impl From<&PkgInfo> for PkgTypeAndName {
    fn from(pkg_info: &PkgInfo) -> Self {
        pkg_info.manifest.type_and_name.clone()
    }
}

impl From<&PkgBasicInfo> for PkgTypeAndName {
    fn from(pkg_basic_info: &PkgBasicInfo) -> Self {
        PkgTypeAndName {
            pkg_type: pkg_basic_info.type_and_name.pkg_type,
            name: pkg_basic_info.type_and_name.name.clone(),
        }
    }
}
