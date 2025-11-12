//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, str::FromStr};

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

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum TenLogLevel {
    Invalid,
    Verbose,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Mandatory,
}

impl FromStr for TenLogLevel {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "V" => Ok(TenLogLevel::Verbose),
            "D" => Ok(TenLogLevel::Debug),
            "I" => Ok(TenLogLevel::Info),
            "W" => Ok(TenLogLevel::Warn),
            "E" => Ok(TenLogLevel::Error),
            "F" => Ok(TenLogLevel::Fatal),
            "M" => Ok(TenLogLevel::Mandatory),
            _ => Err(()),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogLineMetadata {
    pub graph_id: Option<String>,
    pub graph_name: Option<String>,
    pub extension: Option<String>,
    pub log_level: Option<TenLogLevel>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogLineInfo {
    pub line: String,
    pub metadata: Option<LogLineMetadata>,
}

/// Parses a log line and returns metadata about the log line.
///
/// This function handles two types of log lines:
/// 1. Graph resources log lines (log level 'M') - updates the
///    graph_resources_log
/// 2. Regular log lines - extracts extension information and returns metadata
pub fn parse_log_line(
    log_message: &str,
    graph_resources_log: &mut GraphResourcesLog,
) -> Option<LogLineMetadata> {
    // Split the log message by whitespace for initial parsing.
    let parts: Vec<&str> = log_message.split_whitespace().collect();
    if parts.len() < 4 {
        // Need at least date, time, process/thread IDs, and log level.
        return None;
    }

    // Check for log level 'M' - it should be in the fourth position after the
    // timestamp and process/thread IDs.
    let log_level_pos = 3;
    if parts.len() <= log_level_pos {
        return None;
    }

    let log_level_str = parts[log_level_pos];

    // If log level is 'M', handle as graph resources log
    if log_level_str == "M" {
        // Find the "[graph resources]" marker.
        if !log_message.contains("[graph resources]") {
            return None;
        }

        // Extract the JSON content after "[graph resources]".
        let json_content = log_message.split("[graph resources]").nth(1)?.trim();

        // Parse the JSON content.
        let json_value: Value = match serde_json::from_str(json_content) {
            Ok(value) => value,
            Err(_) => return None,
        };

        // Extract data from the JSON.
        let app_base_dir = json_value["app_base_dir"].as_str()?;
        let app_uri = json_value.get("app_uri").and_then(|v| v.as_str());
        let graph_id = json_value["graph_id"].as_str()?;
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

                            let thread_info = ExtensionThreadInfo {
                                extensions: extension_names,
                            };

                            graph_resources_log
                                .extension_threads
                                .insert(thread_id.to_string(), thread_info);
                        }
                    }
                }
            }
        }

        // Successfully parsed as graph resources log, but no metadata to
        // return.
        return None;
    }

    // Handle regular log lines (non-'M' log levels)
    if parts.len() < 5 {
        // Need at least date, time, process/thread IDs, log level, and content.
        return None;
    }

    // Parse the log level, if parsing fails, return None
    let log_level = match log_level_str.parse::<TenLogLevel>() {
        Ok(level) => Some(level),
        Err(_) => return None,
    };

    // Extract the process ID and thread ID.
    // Format expected: "processID(threadID)".
    let process_thread_part = parts[2];
    if !process_thread_part.contains('(') || !process_thread_part.contains(')') {
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

    // Find the content part (everything after the function name part).
    let function_part = parts[log_level_pos + 1]; // This is the function@file:line part.
    if !function_part.contains('@') {
        return None;
    }

    // The content starts from the position after the function part.
    let content_index = log_message.find(function_part)? + function_part.len();
    let content = log_message[content_index..].trim();

    // Try to extract extension name from [...], if not found, set
    // extension_name to None.
    let extension_name = if content.starts_with('[') && content.contains(']') {
        // Extract the extension name from [...].
        let end_pos = content.find(']')?;
        if end_pos <= 1 {
            return None; // No content inside brackets.
        }

        let extracted_name = &content[1..end_pos];

        // Check if extension_name exists in the extension_threads for the given
        // thread_id.
        if let Some(thread_info) = graph_resources_log.extension_threads.get(thread_id) {
            if thread_info.extensions.contains(&extracted_name.to_string()) {
                Some(extracted_name.to_string())
            } else {
                None
            }
        } else {
            None
        }
    } else {
        None
    };

    // Create and return LogLineMetadata.
    Some(LogLineMetadata {
        graph_id: Some(graph_resources_log.graph_id.clone()),
        graph_name: graph_resources_log.graph_name.clone(),
        extension: extension_name,
        log_level,
    })
}

/// Process a log line using the integrated parse_log_line function.
pub fn process_log_line(
    log_line: &str,
    graph_resources_log: &mut GraphResourcesLog,
) -> Option<LogLineMetadata> {
    parse_log_line(log_line, graph_resources_log)
}
