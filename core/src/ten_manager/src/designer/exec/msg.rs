//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
pub enum InboundMsg {
    #[serde(rename = "exec_cmd")]
    ExecCmd { base_dir: String, cmd: String },

    #[serde(rename = "run_script")]
    RunScript { base_dir: String, name: String },
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
pub enum OutboundMsg {
    #[serde(rename = "stdout")]
    StdOut { data: String },

    #[serde(rename = "stderr")]
    StdErr { data: String },

    #[serde(rename = "exit")]
    Exit { code: i32 },

    #[serde(rename = "error")]
    Error { msg: String },
}
