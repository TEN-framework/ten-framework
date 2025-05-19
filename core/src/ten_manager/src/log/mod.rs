//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use serde_json::Value;

pub struct ExtensionThreadInfo {
    pub extensions: Vec<String>,
}

pub struct GraphResourcesLog {
    pub app_base_dir: String,
    pub app_uri: Option<String>,
    pub graph_id: String,
    pub graph_name: Option<String>,
    pub extension_threads: HashMap<String, ExtensionThreadInfo>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogLineMetadata {
    pub graph_id: Option<String>,
    pub graph_name: Option<String>,
    pub extension: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogLineInfo {
    pub line: String,
    pub metadata: Option<LogLineMetadata>,
}

/// Extracts extension information from a log line.
///
/// This function checks if the log line contains extension information in the
/// format "[extension_name]" and returns metadata about the extension if found
/// in the graph resources.
pub fn extract_extension_from_log_line(
    log_message: &str,
    graph_resources_log: &GraphResourcesLog,
) -> Option<LogLineMetadata> {
    // Split the log message by whitespace for initial parsing.
    let parts: Vec<&str> = log_message.split_whitespace().collect();
    if parts.len() < 5 {
        // Need at least date, time, process/thread IDs, log level, and content.
        return None;
    }

    // Extract the process ID and thread ID.
    // Format expected: "processID(threadID)".
    let process_thread_part = parts[2];
    if !process_thread_part.contains('(') || !process_thread_part.contains(')')
    {
        return None;
    }

    let thread_id = process_thread_part
        .split('(')
        .nth(1)? // Get the part after '('.
        .trim_end_matches(')'); // Remove the trailing ')'.

    // Check if the thread ID exists in graph_resources_log.extension_threads.
    if !graph_resources_log.extension_threads.contains_key(thread_id) {
        return None;
    }

    // Check if the log level is 'M', if so, return None.
    let log_level_pos = 3;
    if parts[log_level_pos] == "M" {
        return None;
    }

    // Find the content part (everything after the function name part).
    let function_part = parts[log_level_pos + 1]; // This is the function@file:line part.
    if !function_part.contains('@') {
        return None;
    }

    // The content starts from the position after the function part.
    let content_index = log_message.find(function_part)? + function_part.len();
    let content = log_message[content_index..].trim();

    // Check if content begins with [...], if not, return None.
    if !content.starts_with('[') || !content.contains(']') {
        return None;
    }

    // Extract the extension name from [...].
    let end_pos = content.find(']')?;
    if end_pos <= 1 {
        return None; // No content inside brackets.
    }

    let extension_name = &content[1..end_pos];

    // Check if extension_name exists in the extension_threads for the given
    // thread_id.
    if let Some(thread_info) =
        graph_resources_log.extension_threads.get(thread_id)
    {
        if thread_info.extensions.contains(&extension_name.to_string()) {
            // Create and return LogLineMetadata.
            return Some(LogLineMetadata {
                graph_id: Some(graph_resources_log.graph_id.clone()),
                graph_name: graph_resources_log.graph_name.clone(),
                extension: Some(extension_name.to_string()),
            });
        }
    }

    None
}

pub fn parse_graph_resources_log(
    log_message: &str,
    graph_resources_log: &mut GraphResourcesLog,
) -> Result<()> {
    // Check if the log level is 'M'.
    let parts: Vec<&str> = log_message.split_whitespace().collect();
    if parts.len() < 4 {
        return Err(anyhow!("Not a valid graph resources log message"));
    }

    // Check for log level 'M' - it should be in the fourth position after the
    // timestamp and process/thread IDs.
    let log_level_pos = 3;
    if parts.len() <= log_level_pos || parts[log_level_pos] != "M" {
        return Err(anyhow!("Not a valid graph resources log message"));
    }

    // Find the "[graph resources]" marker.
    if !log_message.contains("[graph resources]") {
        return Err(anyhow!("Not a valid graph resources log message"));
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
    let app_base_dir = json_value["app_base_dir"]
        .as_str()
        .ok_or_else(|| anyhow!("Missing app_base_dir"))?;
    let app_uri = json_value.get("app_uri").and_then(|v| v.as_str());
    let graph_id = json_value["graph_id"]
        .as_str()
        .ok_or_else(|| anyhow!("Missing graph_id"))?;
    let graph_name = json_value.get("graph_name").and_then(|v| v.as_str());

    // Update graph_resources_log with graph ID and name.
    graph_resources_log.graph_id = graph_id.to_string();
    graph_resources_log.graph_name = graph_name.map(|s| s.to_string());
    graph_resources_log.app_base_dir = app_base_dir.to_string();
    graph_resources_log.app_uri = app_uri.map(|s| s.to_string());

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

                        graph_resources_log
                            .extension_threads
                            .insert(thread_id.to_string(), thread_info);
                    }
                }
            }
        }
    }

    Ok(())
}

/// Process a log line: try to parse as graph resources log first, then try to
/// extract extension information.
pub fn process_log_line(
    log_line: &str,
    graph_resources_log: &mut GraphResourcesLog,
) -> Option<LogLineMetadata> {
    // First try to parse as graph resources log.
    match parse_graph_resources_log(log_line, graph_resources_log) {
        Ok(_) => {
            // Successfully parsed as graph resources log, but no metadata to
            // return.
            None
        }
        Err(_) => {
            // Not a graph resources log, try to extract extension information.
            extract_extension_from_log_line(log_line, graph_resources_log)
        }
    }
}
