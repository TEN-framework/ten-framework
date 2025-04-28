//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fmt, str::FromStr};

use anyhow::{Error, Result};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, Eq)]
pub enum ValueType {
    #[serde(rename = "null")]
    Null,

    #[serde(rename = "bool")]
    Bool,

    #[serde(rename = "int8")]
    Int8,

    #[serde(rename = "int16")]
    Int16,

    #[serde(rename = "int32")]
    Int32,

    #[serde(rename = "int64")]
    Int64,

    #[serde(rename = "uint8")]
    Uint8,

    #[serde(rename = "uint16")]
    Uint16,

    #[serde(rename = "uint32")]
    Uint32,

    #[serde(rename = "uint64")]
    Uint64,

    #[serde(rename = "float32")]
    Float32,

    #[serde(rename = "float64")]
    Float64,

    #[serde(rename = "string")]
    String,

    #[serde(rename = "buf")]
    Buf,

    #[serde(rename = "array")]
    Array,

    #[serde(rename = "object")]
    Object,

    #[serde(rename = "ptr")]
    Ptr,
}

impl FromStr for ValueType {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "null" => Ok(ValueType::Null),
            "bool" => Ok(ValueType::Bool),

            "int8" => Ok(ValueType::Int8),
            "int16" => Ok(ValueType::Int16),
            "int32" => Ok(ValueType::Int32),
            "int64" => Ok(ValueType::Int64),

            "uint8" => Ok(ValueType::Uint8),
            "uint16" => Ok(ValueType::Uint16),
            "uint32" => Ok(ValueType::Uint32),
            "uint64" => Ok(ValueType::Uint64),

            "float32" => Ok(ValueType::Float32),
            "float64" => Ok(ValueType::Float64),

            "string" => Ok(ValueType::String),
            "buf" => Ok(ValueType::Buf),

            "array" => Ok(ValueType::Array),
            "object" => Ok(ValueType::Object),

            "ptr" => Ok(ValueType::Ptr),

            _ => Err(Error::msg(format!(
                "Failed to parse string '{}' to value type",
                s
            ))),
        }
    }
}

impl fmt::Display for ValueType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            ValueType::Null => write!(f, "null"),
            ValueType::Bool => write!(f, "bool"),

            ValueType::Int8 => write!(f, "int8"),
            ValueType::Int16 => write!(f, "int16"),
            ValueType::Int32 => write!(f, "int32"),
            ValueType::Int64 => write!(f, "int64"),

            ValueType::Uint8 => write!(f, "uint8"),
            ValueType::Uint16 => write!(f, "uint16"),
            ValueType::Uint32 => write!(f, "uint32"),
            ValueType::Uint64 => write!(f, "uint64"),

            ValueType::Float32 => write!(f, "float32"),
            ValueType::Float64 => write!(f, "float64"),

            ValueType::String => write!(f, "string"),
            ValueType::Buf => write!(f, "buf"),

            ValueType::Array => write!(f, "array"),
            ValueType::Object => write!(f, "object"),

            ValueType::Ptr => write!(f, "ptr"),
        }
    }
}
