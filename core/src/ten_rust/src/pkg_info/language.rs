//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fmt::{self},
    str::FromStr,
};

use anyhow::{Error, Result};
use serde::{Deserialize, Serialize};

#[derive(
    Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize,
)]
pub enum PkgLanguage {
    #[serde(rename = "c")]
    C,

    #[serde(rename = "cpp")]
    Cpp,

    #[serde(rename = "go")]
    Go,

    #[serde(rename = "python")]
    Python,
}

impl FromStr for PkgLanguage {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "c" => Ok(PkgLanguage::C),
            "cpp" => Ok(PkgLanguage::Cpp),
            "go" => Ok(PkgLanguage::Go),
            "python" => Ok(PkgLanguage::Python),
            _ => Err(Error::msg("Failed to parse string to package language")),
        }
    }
}

impl fmt::Display for PkgLanguage {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            PkgLanguage::C => write!(f, "c"),
            PkgLanguage::Cpp => write!(f, "cpp"),
            PkgLanguage::Go => write!(f, "go"),
            PkgLanguage::Python => write!(f, "python"),
        }
    }
}
