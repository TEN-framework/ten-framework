//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fmt, str::FromStr};

use anyhow::{Error, Result};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum MsgDirection {
    #[serde(rename = "in")]
    In,

    #[serde(rename = "out")]
    Out,
}

impl MsgDirection {
    // Method to toggle the value.
    pub fn toggle(&mut self) {
        *self = match *self {
            MsgDirection::In => MsgDirection::Out,
            MsgDirection::Out => MsgDirection::In,
        };
    }
}

impl FromStr for MsgDirection {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "in" => Ok(MsgDirection::In),
            "out" => Ok(MsgDirection::Out),
            _ => Err(Error::msg("Failed to parse string to message direction")),
        }
    }
}

impl fmt::Display for MsgDirection {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            MsgDirection::In => write!(f, "in"),
            MsgDirection::Out => write!(f, "out"),
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum MsgType {
    #[serde(rename = "cmd")]
    Cmd,

    #[serde(rename = "data")]
    Data,

    #[serde(rename = "audio_frame")]
    AudioFrame,

    #[serde(rename = "video_frame")]
    VideoFrame,
}

impl FromStr for MsgType {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "cmd" => Ok(MsgType::Cmd),
            "data" => Ok(MsgType::Data),
            "audio_frame" => Ok(MsgType::AudioFrame),
            "video_frame" => Ok(MsgType::VideoFrame),
            _ => Err(Error::msg("Failed to parse string to message type")),
        }
    }
}

impl fmt::Display for MsgType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            MsgType::Cmd => write!(f, "cmd"),
            MsgType::Data => write!(f, "data"),
            MsgType::AudioFrame => write!(f, "audio frame"),
            MsgType::VideoFrame => write!(f, "video frame"),
        }
    }
}
