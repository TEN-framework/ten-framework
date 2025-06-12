//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::path::{Path, PathBuf};

use anyhow::{anyhow, Context, Result};
use serde::{Deserialize, Serialize};
use url::Url;

use crate::fs::read_file_to_string;
use crate::pkg_info::pkg_type::PkgType;

use super::Graph;

/// Loads graph data from the specified URI with an optional base directory.
///
/// The URI can be:
/// - A relative path (relative to the base_dir if provided)
/// - A URI (http:// or https:// or file://)
///
/// This function returns the loaded Graph structure.
pub fn load_graph_from_uri(
    uri: &str,
    base_dir: Option<&str>,
    new_base_dir: &mut Option<String>,
) -> Result<Graph> {
    // Try to parse as URL first
    if let Ok(url) = Url::parse(uri) {
        match url.scheme() {
            "http" | "https" => {
                return load_graph_from_http_url(&url, new_base_dir);
            }
            "file" => {
                return load_graph_from_file_url(&url, new_base_dir);
            }
            _ => {
                return Err(anyhow!(
                    "Unsupported URL scheme '{}' in import_uri: {}",
                    url.scheme(),
                    uri
                ));
            }
        }
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

/// Loads graph data from an HTTP/HTTPS URL.
async fn load_graph_from_http_url_async(
    url: &Url,
    new_base_dir: &mut Option<String>,
) -> Result<Graph> {
    // Create HTTP client
    let client = reqwest::Client::new();

    // Make HTTP request
    let response =
        client.get(url.as_str()).send().await.with_context(|| {
            format!("Failed to send HTTP request to {}", url)
        })?;

    // Check if request was successful
    if !response.status().is_success() {
        return Err(anyhow!(
            "HTTP request failed with status {}: {}",
            response.status(),
            url
        ));
    }

    // Get response body as text
    let graph_content = response.text().await.with_context(|| {
        format!("Failed to read response body from {}", url)
    })?;

    // Set the new_base_dir to the directory part of the URL
    if new_base_dir.is_some() {
        let mut base_url = url.clone();
        // Remove the file part from the URL to get the base directory
        if let Ok(mut segments) = base_url.path_segments_mut() {
            segments.pop();
        }
        *new_base_dir = Some(base_url.to_string());
    }

    // Parse the graph file into a Graph structure.
    let graph: Graph = serde_json::from_str(&graph_content)
        .with_context(|| format!("Failed to parse graph JSON from {}", url))?;

    Ok(graph)
}

/// Synchronous wrapper for HTTP URL loading.
fn load_graph_from_http_url(
    url: &Url,
    new_base_dir: &mut Option<String>,
) -> Result<Graph> {
    // Use tokio runtime to execute async HTTP request
    let rt = tokio::runtime::Runtime::new()
        .context("Failed to create tokio runtime")?;

    rt.block_on(load_graph_from_http_url_async(url, new_base_dir))
}

/// Loads graph data from a file:// URL.
fn load_graph_from_file_url(
    url: &Url,
    new_base_dir: &mut Option<String>,
) -> Result<Graph> {
    // Convert file URL to local path
    let path =
        url.to_file_path().map_err(|_| anyhow!("Invalid file URL: {}", url))?;

    // Set the new_base_dir to the directory containing the file
    if let Some(parent_dir) = path.parent() {
        if new_base_dir.is_some() {
            *new_base_dir = Some(parent_dir.to_string_lossy().to_string());
        }
    }

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
    pub import_uri: Option<String>,

    #[serde(skip)]
    pub app_base_dir: Option<String>,
    #[serde(skip)]
    pub belonging_pkg_type: Option<PkgType>,
    #[serde(skip)]
    pub belonging_pkg_name: Option<String>,
}

impl GraphInfo {
    pub fn validate_and_complete_and_flatten(&mut self) -> Result<()> {
        // Validate mutual exclusion between import_uri and graph fields
        if self.import_uri.is_some() {
            // When import_uri is present, the graph fields should be empty or
            // None
            if !self.graph.nodes.is_empty() {
                return Err(anyhow!(
                    "When 'import_uri' is specified, 'nodes' field must not \
                     be present"
                ));
            }

            if let Some(connections) = &self.graph.connections {
                if !connections.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'connections' field \
                         must not be present"
                    ));
                }
            }

            if let Some(exposed_messages) = &self.graph.exposed_messages {
                if !exposed_messages.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'exposed_messages' \
                         field must not be present"
                    ));
                }
            }

            if let Some(exposed_properties) = &self.graph.exposed_properties {
                if !exposed_properties.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'exposed_properties' \
                         field must not be present"
                    ));
                }
            }
        }

        // If import_uri is specified, load graph from the URI.
        let import_uri = self.import_uri.clone();
        let app_base_dir = self.app_base_dir.clone();
        if let Some(import_uri) = import_uri {
            // Load graph from URI and replace the current graph
            let graph = load_graph_from_uri(
                &import_uri,
                app_base_dir.as_deref(),
                &mut None,
            )?;
            self.graph = graph;
        }

        self.graph.validate_and_complete_and_flatten(app_base_dir.as_deref())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn test_load_graph_from_file_url() {
        // Create a temporary directory and file
        let temp_dir = tempdir().unwrap();
        let file_path = temp_dir.path().join("test_graph.json");

        // Create a simple test graph
        let test_graph = r#"{
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension",
                    "addon": "test_addon",
                    "app": "localhost"
                }
            ]
        }"#;

        fs::write(&file_path, test_graph).unwrap();

        // Create a file:// URL
        let file_url = format!("file://{}", file_path.display());

        // Test loading the graph
        let mut new_base_dir = Some(String::new());
        let result = load_graph_from_uri(&file_url, None, &mut new_base_dir);

        assert!(result.is_ok());
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 1);
        assert_eq!(graph.nodes[0].name, "test_extension");

        // Check that new_base_dir was set correctly
        assert!(new_base_dir.is_some());
        let base_dir = new_base_dir.unwrap();
        assert_eq!(base_dir, temp_dir.path().to_string_lossy());
    }

    #[test]
    fn test_load_graph_from_relative_path() {
        // Create a temporary directory and file
        let temp_dir = tempdir().unwrap();
        let file_path = temp_dir.path().join("test_graph.json");

        // Create a simple test graph
        let test_graph = r#"{
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension",
                    "addon": "test_addon",
                    "app": "localhost"
                }
            ]
        }"#;

        fs::write(&file_path, test_graph).unwrap();

        // Test loading with relative path
        let mut new_base_dir = Some(String::new());
        let result = load_graph_from_uri(
            "test_graph.json",
            Some(&temp_dir.path().to_string_lossy()),
            &mut new_base_dir,
        );

        assert!(result.is_ok());
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 1);
        assert_eq!(graph.nodes[0].name, "test_extension");

        // Check that new_base_dir was set correctly
        assert!(new_base_dir.is_some());
        let base_dir = new_base_dir.unwrap();
        assert_eq!(base_dir, temp_dir.path().to_string_lossy());
    }

    #[test]
    fn test_load_graph_from_absolute_path() {
        // Create a temporary directory and file
        let temp_dir = tempdir().unwrap();
        let file_path = temp_dir.path().join("test_graph.json");

        // Create a simple test graph
        let test_graph = r#"{
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension",
                    "addon": "test_addon",
                    "app": "localhost"
                }
            ]
        }"#;

        fs::write(&file_path, test_graph).unwrap();

        // Test loading with absolute path
        let mut new_base_dir = Some(String::new());
        let result = load_graph_from_uri(
            &file_path.to_string_lossy(),
            None,
            &mut new_base_dir,
        );

        assert!(result.is_ok());
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 1);
        assert_eq!(graph.nodes[0].name, "test_extension");
    }

    #[test]
    fn test_unsupported_url_scheme() {
        let mut new_base_dir = Some(String::new());
        let result = load_graph_from_uri(
            "ftp://example.com/graph.json",
            None,
            &mut new_base_dir,
        );

        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg.contains("Unsupported URL scheme 'ftp'"));
    }

    #[test]
    fn test_relative_path_without_base_dir() {
        let mut new_base_dir = Some(String::new());
        let result =
            load_graph_from_uri("test_graph.json", None, &mut new_base_dir);

        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg
            .contains("base_dir cannot be None when uri is a relative path"));
    }
}
