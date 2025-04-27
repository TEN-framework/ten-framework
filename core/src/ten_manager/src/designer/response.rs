//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::{Error, Result};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum Status {
    #[serde(rename = "ok")]
    Ok,

    #[serde(rename = "fail")]
    Fail,
}

impl From<Status> for String {
    fn from(status: Status) -> Self {
        match status {
            Status::Ok => "ok".to_string(),
            Status::Fail => "fail".to_string(),
        }
    }
}

fn status_to_string<S>(
    status: &Status,
    serializer: S,
) -> Result<S::Ok, S::Error>
where
    S: serde::Serializer,
{
    let status_str: String = status.clone().into();
    serializer.serialize_str(&status_str)
}

#[derive(Serialize, Deserialize, Debug)]
pub struct ApiResponse<T> {
    #[serde(serialize_with = "status_to_string")]
    pub status: Status,

    pub data: T,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub meta: Option<Meta>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Meta {}

#[derive(Serialize, Deserialize, Debug)]
pub struct ErrorResponse {
    #[serde(serialize_with = "status_to_string")]
    pub status: Status,

    pub message: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<InnerError>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct InnerError {
    #[serde(rename = "type")]
    pub error_type: String,
    pub code: Option<String>,
    pub message: String,
}

impl ErrorResponse {
    pub fn from_error(err: &Error, message_header: &str) -> Self {
        let mut this = ErrorResponse {
            status: Status::Fail,
            message: format!("{} {}", message_header, err),
            error: None,
        };

        let mut errors = vec![];

        err.chain().skip(1).for_each(|cause| {
            errors.push(cause.to_string());
        });

        if !errors.is_empty() {
            this.error = Some(InnerError {
                error_type: "root cause".to_string(),
                code: None,
                message: errors.join("\n"),
            });
        }

        this
    }
}
