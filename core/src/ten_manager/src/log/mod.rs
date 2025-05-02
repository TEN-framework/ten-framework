//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::{anyhow, Result};
use serde_json::Value;
use std::collections::HashMap;

pub struct ExtensionThreadInfo {
    pub extensions: Vec<String>,
}

pub struct AppInfo {
    pub extension_threads: HashMap<String, ExtensionThreadInfo>,
}

pub struct GraphResourcesLog {
    pub graph_id: String,
    pub graph_name: String,
    pub apps: HashMap<Option<String>, AppInfo>,
}

pub fn parse_graph_resources_log(
    log_message: &str,
    graph_resources_log: &mut GraphResourcesLog,
) -> Result<()> {
    // Check if the log level is 'M'.
    let parts: Vec<&str> = log_message.split_whitespace().collect();
    if parts.len() < 4 {
        return Ok(());
    }

    // Check for log level 'M' - it should be in the fourth position after the
    // timestamp and process/thread IDs.
    let log_level_pos = 3;
    if parts.len() <= log_level_pos || parts[log_level_pos] != "M" {
        return Ok(());
    }

    // Find the "[graph resources]" marker.
    if !log_message.contains("[graph resources]") {
        return Ok(());
    }

    // Extract the JSON content after "[graph resources]".
    let json_content = log_message
        .split("[graph resources]")
        .nth(1)
        .ok_or_else(|| anyhow!("Failed to extract JSON content"))?
        .trim();

    // Parse the JSON content.
    let json_value: Value = serde_json::from_str(json_content)?;

    // Extract data from the JSON.
    let app_uri = json_value.get("app_uri").and_then(|v| v.as_str());
    let graph_id = json_value["graph id"]
        .as_str()
        .ok_or_else(|| anyhow!("Missing graph id"))?;
    let graph_name = json_value["graph name"]
        .as_str()
        .ok_or_else(|| anyhow!("Missing graph name"))?;

    // Update graph_resources_log with graph ID and name.
    graph_resources_log.graph_id = graph_id.to_string();
    graph_resources_log.graph_name = graph_name.to_string();

    // Create or get the AppInfo for this app_uri.
    let app_key = app_uri.map(|uri| uri.to_string());
    let app_info = graph_resources_log
        .apps
        .entry(app_key)
        .or_insert_with(|| AppInfo { extension_threads: HashMap::new() });

    // Process extension_threads if present.
    if let Some(extension_threads) = json_value.get("extension_threads") {
        if let Some(extension_threads_obj) = extension_threads.as_object() {
            for (thread_id, thread_info) in extension_threads_obj {
                if let Some(extensions_array) = thread_info.get("extensions") {
                    if let Some(extensions) = extensions_array.as_array() {
                        let mut extension_names = Vec::new();
                        for ext in extensions {
                            if let Some(ext_name) = ext.as_str() {
                                extension_names.push(ext_name.to_string());
                            }
                        }

                        let thread_info =
                            ExtensionThreadInfo { extensions: extension_names };

                        app_info
                            .extension_threads
                            .insert(thread_id.to_string(), thread_info);
                    }
                }
            }
        }
    }

    Ok(())
}
