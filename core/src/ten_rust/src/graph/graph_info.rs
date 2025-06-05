//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::path::{Path, PathBuf};

use anyhow::{anyhow, Context, Result};
use serde::{Deserialize, Serialize};

use crate::fs::read_file_to_string;
use crate::pkg_info::pkg_type::PkgType;

use super::Graph;

/// Loads graph data from the specified URI with an optional base directory.
///
/// The URI can be:
/// - A relative path (relative to the base_dir if provided)
/// - An absolute path
/// - A URL
///
/// This function returns the loaded Graph structure.
pub fn load_graph_from_uri(
    uri: &str,
    base_dir: Option<&str>,
    new_base_dir: &mut Option<String>,
) -> Result<Graph> {
    // Check if the URI is a URL (starts with http:// or https://)
    if uri.starts_with("http://") || uri.starts_with("https://") {
        // TODO: Implement HTTP request to fetch the graph file
        // For now, return an error since HTTP requests are not implemented
        // yet.
        return Err(anyhow!("HTTP URLs are not supported yet for source_uri"));
    }

    // Handle relative and absolute paths.
    let path = if Path::new(uri).is_absolute() {
        PathBuf::from(uri)
    } else {
        // For relative paths, base_dir must not be None
        let base_dir = base_dir.ok_or_else(|| {
            anyhow!("base_dir cannot be None when uri is a relative path")
        })?;

        // If base_dir is available, use it as the base for relative paths.
        let new_path = Path::new(base_dir).join(uri);

        // Set the new_base_dir to the directory containing the resolved path
        if let Some(parent_dir) = new_path.parent() {
            if new_base_dir.is_some() {
                *new_base_dir = Some(parent_dir.to_string_lossy().to_string());
            }
        }

        new_path
    };

    // Read the graph file.
    let graph_content = read_file_to_string(&path).with_context(|| {
        format!("Failed to read graph file from {}", path.display())
    })?;

    // Parse the graph file into a Graph structure.
    let graph: Graph =
        serde_json::from_str(&graph_content).with_context(|| {
            format!("Failed to parse graph file from {}", path.display())
        })?;

    Ok(graph)
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphInfo {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub auto_start: Option<bool>,

    #[serde(flatten)]
    pub graph: Graph,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub source_uri: Option<String>,

    #[serde(skip)]
    pub app_base_dir: Option<String>,
    #[serde(skip)]
    pub belonging_pkg_type: Option<PkgType>,
    #[serde(skip)]
    pub belonging_pkg_name: Option<String>,
}

impl GraphInfo {
    pub fn validate_and_complete_and_flatten(&mut self) -> Result<()> {
        // Validate mutual exclusion between source_uri and graph fields
        if self.source_uri.is_some() {
            // When source_uri is present, the graph fields should be empty or
            // None
            if !self.graph.nodes.is_empty() {
                return Err(anyhow!(
                    "When 'source_uri' is specified, 'nodes' field must not \
                     be present"
                ));
            }

            if let Some(connections) = &self.graph.connections {
                if !connections.is_empty() {
                    return Err(anyhow!(
                        "When 'source_uri' is specified, 'connections' field \
                         must not be present"
                    ));
                }
            }

            if let Some(exposed_messages) = &self.graph.exposed_messages {
                if !exposed_messages.is_empty() {
                    return Err(anyhow!(
                        "When 'source_uri' is specified, 'exposed_messages' \
                         field must not be present"
                    ));
                }
            }

            if let Some(exposed_properties) = &self.graph.exposed_properties {
                if !exposed_properties.is_empty() {
                    return Err(anyhow!(
                        "When 'source_uri' is specified, 'exposed_properties' \
                         field must not be present"
                    ));
                }
            }
        }

        // If source_uri is specified, load graph from the URI.
        let source_uri = self.source_uri.clone();
        let app_base_dir = self.app_base_dir.clone();
        if let Some(source_uri) = source_uri {
            // Load graph from URI and replace the current graph
            let graph = load_graph_from_uri(
                &source_uri,
                app_base_dir.as_deref(),
                &mut None,
            )?;
            self.graph = graph;
        }

        self.graph.validate_and_complete_and_flatten(app_base_dir.as_deref())
    }
}
