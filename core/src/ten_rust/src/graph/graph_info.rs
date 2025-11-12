//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use anyhow::{anyhow, Context, Result};
use serde::{Deserialize, Serialize};

use super::Graph;
use crate::{
    pkg_info::pkg_type::PkgType,
    utils::{
        path::{get_base_dir_of_uri, get_real_path_from_import_uri},
        uri::load_content_from_uri,
    },
};

/// Loads graph data from the specified URI with an optional base directory.
///
/// The URI can be:
/// - A relative path (relative to the base_dir if provided)
/// - A URI (http:// or https:// or file://)
///
/// TODO(Wei): Absolute file paths are NOT supported. Use file:// URI instead.
/// According to the uri-reference specification, absolute file paths require
/// special handling. For example, on Windows, absolute paths need to start with
/// a forward slash, like /c:/..., so simply using Path::new(uri).is_absolute()
/// is insufficient and requires additional consideration.
///
/// This function returns the loaded Graph structure.
pub async fn load_graph_from_uri(
    uri: &str,
    base_dir: Option<&str>,
    new_base_dir: &mut Option<String>,
) -> Result<Graph> {
    // Get the real path of the import_uri based on the base_dir.
    let real_path = get_real_path_from_import_uri(uri, base_dir, None)?;

    // Read the graph file.
    let graph_content = load_content_from_uri(&real_path).await?;

    *new_base_dir = Some(get_base_dir_of_uri(&real_path)?);

    // Parse the graph file into a Graph structure.
    let graph: Graph = serde_json::from_str(&graph_content)
        .with_context(|| format!("Failed to parse graph file from {real_path}"))?;

    Ok(graph)
}

/// Represents the content of a graph field in predefined_graphs.
/// This can either contain an import_uri or direct graph content (both original
/// and flattened).
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphContent {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub import_uri: Option<String>,

    #[serde(flatten)]
    pub graph: Graph,

    #[serde(skip)]
    pub flattened_graph: Option<Graph>,
}

impl GraphContent {
    /// Get a reference to the nodes
    pub fn nodes(&self) -> &Vec<crate::graph::node::GraphNode> {
        &self.graph.nodes
    }

    /// Get a mutable reference to the nodes
    pub fn nodes_mut(&mut self) -> &mut Vec<crate::graph::node::GraphNode> {
        &mut self.graph.nodes
    }

    /// Get a reference to the connections
    pub fn connections(&self) -> &Option<Vec<crate::graph::connection::GraphConnection>> {
        &self.graph.connections
    }

    /// Get a mutable reference to the connections
    pub fn connections_mut(
        &mut self,
    ) -> &mut Option<Vec<crate::graph::connection::GraphConnection>> {
        &mut self.graph.connections
    }

    /// Get a reference to the exposed_messages
    pub fn exposed_messages(&self) -> &Option<Vec<crate::graph::GraphExposedMessage>> {
        &self.graph.exposed_messages
    }

    /// Get a mutable reference to the exposed_messages
    pub fn exposed_messages_mut(&mut self) -> &mut Option<Vec<crate::graph::GraphExposedMessage>> {
        &mut self.graph.exposed_messages
    }

    /// Get a reference to the exposed_properties
    pub fn exposed_properties(&self) -> &Option<Vec<crate::graph::GraphExposedProperty>> {
        &self.graph.exposed_properties
    }

    /// Get a mutable reference to the exposed_properties
    pub fn exposed_properties_mut(
        &mut self,
    ) -> &mut Option<Vec<crate::graph::GraphExposedProperty>> {
        &mut self.graph.exposed_properties
    }

    /// Get a reference to the inner Graph
    pub fn graph(&self) -> &Graph {
        &self.graph
    }

    /// Get a mutable reference to the inner Graph
    pub fn graph_mut(&mut self) -> &mut Graph {
        &mut self.graph
    }

    pub async fn validate_and_complete_and_flatten(
        &mut self,
        current_base_dir: Option<&str>,
    ) -> Result<()> {
        // Validate mutual exclusion between import_uri and graph fields
        if self.import_uri.is_some() {
            // When import_uri is present, the graph fields should be empty or
            // None
            if !self.graph.nodes.is_empty() {
                return Err(anyhow!(
                    "When 'import_uri' is specified, 'nodes' field must not be present"
                ));
            }

            if let Some(connections) = &self.graph.connections {
                if !connections.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'connections' field must not be present"
                    ));
                }
            }

            if let Some(exposed_messages) = &self.graph.exposed_messages {
                if !exposed_messages.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'exposed_messages' field must not be \
                         present"
                    ));
                }
            }

            if let Some(exposed_properties) = &self.graph.exposed_properties {
                if !exposed_properties.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'exposed_properties' field must not be \
                         present"
                    ));
                }
            }
        }

        // If import_uri is specified, load graph from the URI.
        if let Some(import_uri) = &self.import_uri {
            // Load graph from URI and replace the current graph
            let graph = load_graph_from_uri(import_uri, current_base_dir, &mut None).await?;
            self.graph = graph;
            self.import_uri = None;
        }

        self.graph.validate_and_complete(current_base_dir)?;

        self.flattened_graph = self.graph.flatten_graph(current_base_dir).await?;

        Ok(())
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphInfo {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub auto_start: Option<bool>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub singleton: Option<bool>,

    pub graph: GraphContent,

    #[serde(skip)]
    pub app_base_dir: Option<String>,
    #[serde(skip)]
    pub belonging_pkg_type: Option<PkgType>,
    #[serde(skip)]
    pub belonging_pkg_name: Option<String>,
}

impl GraphInfo {
    pub async fn from_str_with_base_dir(s: &str, current_base_dir: Option<&str>) -> Result<Self> {
        let mut graph_info: GraphInfo = serde_json::from_str(s)?;
        graph_info.app_base_dir = current_base_dir.map(|s| s.to_string());
        graph_info.validate_and_complete_and_flatten().await?;
        // Return the parsed data.
        Ok(graph_info)
    }

    pub async fn validate_and_complete_and_flatten(&mut self) -> Result<()> {
        self.graph.validate_and_complete_and_flatten(self.app_base_dir.as_deref()).await
    }

    pub async fn to_json_with_flattened_graph(&mut self) -> Result<String> {
        self.validate_and_complete_and_flatten().await?;
        let mut graph_info = self.clone();
        // Replace the graph with the flattened graph if the flattened graph exists.
        if let Some(flattened_graph) = self.graph.flattened_graph.as_ref() {
            graph_info.graph.graph = flattened_graph.clone();
        }
        let json = serde_json::to_string(&graph_info)?;
        Ok(json)
    }
}
