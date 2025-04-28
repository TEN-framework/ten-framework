//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fmt::{self, Display},
    str::FromStr,
};

use serde::{Deserialize, Serialize};

#[derive(
    Debug, PartialEq, Eq, Clone, Copy, Serialize, Deserialize, Default,
)]
pub enum Locale {
    #[default]
    #[serde(rename = "en-US")]
    EnUs,

    #[serde(rename = "zh-CN")]
    ZhCn,

    #[serde(rename = "zh-TW")]
    ZhTw,

    #[serde(rename = "ja-JP")]
    JaJp,
}

impl Display for Locale {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            Locale::EnUs => "en-US",
            Locale::ZhCn => "zh-CN",
            Locale::ZhTw => "zh-TW",
            Locale::JaJp => "ja-JP",
        };
        write!(f, "{}", s)
    }
}

impl FromStr for Locale {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "en-US" => Ok(Locale::EnUs),
            "zh-CN" => Ok(Locale::ZhCn),
            "zh-TW" => Ok(Locale::ZhTw),
            "ja-JP" => Ok(Locale::JaJp),
            _ => Err(format!("Invalid locale: {}", s)),
        }
    }
}
