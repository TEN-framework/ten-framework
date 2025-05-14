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
    pub fn validate_and_complete(&mut self) -> Result<()> {
        // If source_uri is specified, load graph from the URI.
        if let Some(uri) = self.source_uri.clone() {
            self.load_graph_from_uri(&uri)?;
        }

        self.graph.validate_and_complete()
    }

    /// Loads graph data from the specified URI.
    ///
    /// The URI can be:
    /// - A relative path (relative to the directory containing property.json)
    /// - An absolute path
    /// - A URL
    ///
    /// This function replaces the current graph with the one loaded from the
    /// URI.
    fn load_graph_from_uri(&mut self, uri: &str) -> Result<()> {
        // Check if the URI is a URL (starts with http:// or https://)
        if uri.starts_with("http://") || uri.starts_with("https://") {
            // TODO: Implement HTTP request to fetch the graph file
            // For now, return an error since HTTP requests are not implemented
            // yet.
            return Err(anyhow!(
                "HTTP URLs are not supported yet for source_uri"
            ));
        }

        // Handle relative and absolute paths.
        let path = if Path::new(uri).is_absolute() {
            PathBuf::from(uri)
        } else if let Some(app_base_dir) = &self.app_base_dir {
            // If app_base_dir is available, use it as the base for relative
            // paths.
            Path::new(app_base_dir).join(uri)
        } else {
            // If app_base_dir is not available, just use the URI as is.
            PathBuf::from(uri)
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

        // Replace the current graph with the loaded one.
        self.graph = graph;

        Ok(())
    }
}
