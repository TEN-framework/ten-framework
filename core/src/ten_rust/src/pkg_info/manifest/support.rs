//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;
use serde::{Deserialize, Serialize};
use std::fmt;

use crate::{
    env::get_env,
    pkg_info::supports::{Arch, Os},
};

#[derive(
    Serialize, Deserialize, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord,
)]
pub struct ManifestSupport {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub os: Option<Os>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub arch: Option<Arch>,
}

impl fmt::Display for ManifestSupport {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let os_str = match &self.os {
            Some(os) => os.to_string(),
            None => "".to_string(),
        };

        let arch_str = match &self.arch {
            Some(arch) => arch.to_string(),
            None => "".to_string(),
        };

        write!(f, "{}, {}", os_str, arch_str)
    }
}

pub struct SupportsDisplay<'a>(pub &'a [ManifestSupport]);

impl fmt::Display for SupportsDisplay<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.0.is_empty() {
            write!(f, "all environments")
        } else {
            for (i, support) in self.0.iter().enumerate() {
                if i > 0 {
                    write!(f, ", ")?;
                }
                write!(f, "({})", support)?;
            }
            Ok(())
        }
    }
}

impl ManifestSupport {
    fn is_support_compatible(&self, pivot: &ManifestSupport) -> i32 {
        let mut compatible_score = 0;

        if let Some(os) = &pivot.os {
            if let Some(support_os) = &self.os {
                if os != support_os {
                    // If a specific OS support is required, and the current
                    // support combination does not list the desired OS, then it
                    // is not compatible.
                    return -1;
                }
                compatible_score += 1;
            }
        }

        if let Some(arch) = &pivot.arch {
            if let Some(support_arch) = &self.arch {
                if arch != support_arch {
                    // Same as above.
                    return -1;
                }
                compatible_score += 1;
            }
        }

        compatible_score
    }

    pub fn set_defaults(&mut self) -> Result<()> {
        let current_env = get_env()?;

        if self.os.is_none() {
            self.os = Some(current_env.os);
        }
        if self.arch.is_none() {
            self.arch = Some(current_env.arch);
        }

        Ok(())
    }
}

pub fn get_manifest_supports_from_pkg(
    support: &[ManifestSupport],
) -> Vec<ManifestSupport> {
    support.to_vec()
}

pub fn is_manifest_supports_compatible_with(
    manifest_supports: &Vec<ManifestSupport>,
    pivot: &ManifestSupport,
) -> i32 {
    if manifest_supports.is_empty() {
        // If no support combinations are listed, then it is compatible with
        // everything.
        return 0;
    }

    let mut largest_compatible_score = -1;

    // Loop through all listed support combinations to see if one of them
    // matches the specified support.
    for support in manifest_supports {
        let new_score = support.is_support_compatible(pivot);
        if new_score > largest_compatible_score {
            largest_compatible_score = new_score;
        }
    }

    largest_compatible_score
}
