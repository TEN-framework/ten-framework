//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::hash::Hash;
use std::{fmt, str::FromStr};

use anyhow::{Error, Result};
use serde::{Deserialize, Serialize};

#[derive(
    Clone, Debug, Serialize, Hash, Deserialize, PartialEq, Eq, PartialOrd, Ord,
)]
pub enum Os {
    #[serde(rename = "win")]
    Win,

    #[serde(rename = "mac")]
    Mac,

    #[serde(rename = "linux")]
    Linux,
}

impl FromStr for Os {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "win" => Ok(Os::Win),
            "windows" => Ok(Os::Win),
            "mac" => Ok(Os::Mac),
            "macos" => Ok(Os::Mac),
            "linux" => Ok(Os::Linux),
            _ => Err(Error::msg("Failed to parse string to OS")),
        }
    }
}

impl fmt::Display for Os {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Os::Win => write!(f, "win"),
            Os::Mac => write!(f, "mac"),
            Os::Linux => write!(f, "linux"),
        }
    }
}

#[derive(
    Clone, Debug, Serialize, Hash, Deserialize, PartialEq, Eq, PartialOrd, Ord,
)]
pub enum Arch {
    #[serde(rename = "x86")]
    X86,

    #[serde(rename = "x64")]
    X64,

    #[serde(rename = "arm")]
    Arm,

    #[serde(rename = "arm64")]
    Arm64,
}

impl FromStr for Arch {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "x86" => Ok(Arch::X86),
            "x64" => Ok(Arch::X64),
            "x86_64" => Ok(Arch::X64),
            "arm" => Ok(Arch::Arm),
            "arm64" => Ok(Arch::Arm64),
            "aarch64" => Ok(Arch::Arm64),
            _ => Err(Error::msg("Failed to parse string to CPU")),
        }
    }
}

impl fmt::Display for Arch {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Arch::X86 => write!(f, "x86"),
            Arch::X64 => write!(f, "x64"),
            Arch::Arm => write!(f, "arm"),
            Arch::Arm64 => write!(f, "arm64"),
        }
    }
}
