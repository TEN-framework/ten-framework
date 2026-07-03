//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use serde::{Deserialize, Serialize};

use crate::log::LogLineInfo;

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
pub enum InboundMsg {
    #[serde(rename = "exec_cmd")]
    ExecCmd { base_dir: String, cmd: String, stdout_is_log: bool, stderr_is_log: bool },

    #[serde(rename = "run_script")]
    RunScript { base_dir: String, name: String, stdout_is_log: bool, stderr_is_log: bool },
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
pub enum OutboundMsg {
    #[serde(rename = "stdout_normal")]
    StdOutNormal { data: String },

    #[serde(rename = "stdout_log")]
    StdOutLog { data: LogLineInfo },

    #[serde(rename = "stderr_normal")]
    StdErrNormal { data: String },

    #[serde(rename = "stderr_log")]
    StdErrLog { data: LogLineInfo },

    #[serde(rename = "exit")]
    Exit { code: i32 },

    #[serde(rename = "error")]
    Error { msg: String },
}
