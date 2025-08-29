//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use actix::Addr;
use serde::{Deserialize, Serialize};

use crate::output::TmanOutput;

use super::{BuiltinFunctionOutput, WsBuiltinFunction};

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
pub enum InboundMsg {
    #[serde(rename = "install_all")]
    InstallAll { base_dir: String },

    #[serde(rename = "install")]
    Install {
        base_dir: String,
        pkg_type: String,
        pkg_name: String,
        pkg_version: Option<String>,
    },
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
pub enum OutboundMsg {
    #[serde(rename = "normal_line")]
    NormalLine { data: String },

    #[serde(rename = "normal_partial")]
    NormalPartial { data: String },

    #[serde(rename = "error_line")]
    ErrorLine { data: String },

    #[serde(rename = "error_partial")]
    ErrorPartial { data: String },

    #[serde(rename = "exit")]
    Exit {
        code: i32,
        error_message: Option<String>,
    },
}

pub struct TmanOutputWs {
    pub addr: Addr<WsBuiltinFunction>,
}

impl TmanOutput for TmanOutputWs {
    fn normal_line(&self, text: &str) {
        self.addr
            .do_send(BuiltinFunctionOutput::NormalLine(text.to_string()));
    }
    fn normal_partial(&self, text: &str) {
        self.addr
            .do_send(BuiltinFunctionOutput::NormalPartial(text.to_string()));
    }
    fn error_line(&self, text: &str) {
        self.addr
            .do_send(BuiltinFunctionOutput::ErrorLine(text.to_string()));
    }
    fn error_partial(&self, text: &str) {
        self.addr
            .do_send(BuiltinFunctionOutput::ErrorPartial(text.to_string()));
    }
    fn is_interactive(&self) -> bool {
        false
    }
}
