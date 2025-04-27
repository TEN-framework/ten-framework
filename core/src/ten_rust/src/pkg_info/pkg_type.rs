//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fmt, str::FromStr};

use anyhow::{Error, Result};
use serde::{Deserialize, Serialize};

#[derive(
    Copy,
    Clone,
    Debug,
    PartialEq,
    Eq,
    PartialOrd,
    Ord,
    Hash,
    Serialize,
    Deserialize,
    Default,
)]
pub enum PkgType {
    #[serde(rename = "invalid")]
    #[default]
    Invalid,

    #[serde(rename = "system")]
    System,

    #[serde(rename = "app")]
    App,

    #[serde(rename = "extension")]
    Extension,

    #[serde(rename = "protocol")]
    Protocol,

    #[serde(rename = "addon_loader")]
    AddonLoader,
}

impl FromStr for PkgType {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "system" => Ok(PkgType::System),
            "app" => Ok(PkgType::App),
            "extension" => Ok(PkgType::Extension),
            "protocol" => Ok(PkgType::Protocol),
            "addon_loader" => Ok(PkgType::AddonLoader),
            _ => Err(Error::msg("Failed to parse string to package type")),
        }
    }
}

impl fmt::Display for PkgType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            PkgType::Invalid => write!(f, "invalid"),
            PkgType::System => write!(f, "system"),
            PkgType::App => write!(f, "app"),
            PkgType::Extension => write!(f, "extension"),
            PkgType::Protocol => write!(f, "protocol"),
            PkgType::AddonLoader => write!(f, "addon_loader"),
        }
    }
}
