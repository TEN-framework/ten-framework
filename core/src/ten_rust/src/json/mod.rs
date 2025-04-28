//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod bindings;

use anyhow::{Context, Result};
use json5;
use serde_json;

fn ten_remove_json_comments(json_with_comments: &str) -> Result<String> {
    let parsed_value: serde_json::Value =
        json5::from_str(json_with_comments)
            .context("Failed to parse the input as JSON5")?;

    let json_str = serde_json::to_string_pretty(&parsed_value)
        .context("Failed to serialize the parsed data into a JSON string")?;

    Ok(json_str)
}
