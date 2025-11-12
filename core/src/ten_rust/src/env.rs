//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::str::FromStr;

use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::pkg_info::supports::{Arch, Os};

#[derive(Serialize, Deserialize)]
pub struct EnvInfo {
    pub os: Os,
    pub arch: Arch,
}

pub fn get_env() -> Result<EnvInfo> {
    let current_os = Os::from_str(std::env::consts::OS)?;
    let current_arch = Arch::from_str(std::env::consts::ARCH)?;

    // Create response object
    let env_info = EnvInfo {
        os: current_os,
        arch: current_arch,
    };

    Ok(env_info)
}
