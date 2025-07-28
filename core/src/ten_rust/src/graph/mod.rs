//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod check;
pub mod connection;
pub mod graph_info;
pub mod msg_conversion;
pub mod node;
pub mod reverse;
pub mod selector;
pub mod subgraph;

use std::collections::HashMap;

use anyhow::Result;
use node::GraphNode;
use serde::{Deserialize, Serialize};

use crate::base_dir_pkg_info::PkgsInfoInApp;
use crate::constants::{
    ERR_MSG_GRAPH_APP_FIELD_EMPTY, ERR_MSG_GRAPH_MIXED_APP_DECLARATIONS,
};
use crate::pkg_info::localhost;

use self::connection::{GraphConnection, GraphMessageFlow};
use self::node::GraphNodeType;

/// The state of the 'app' field declaration in all nodes in the graph.
///
/// There might be the following cases for the 'app' field declaration:
///
/// - Case 1: neither of the nodes has declared the 'app' field. The state will
///   be `NoneDeclared`.
///
/// - Case 2: all nodes have declared the 'app' field, and all of them have the
///   same value. Ex:
///
/// {
///   "nodes": [
///     {
///       "type": "extension",
///       "app": "http://localhost:8000",
///       "addon": "addon_1",
///       "name": "ext_1",
///       "extension_group": "some_group"
///     },
///     {
///       "type": "extension",
///       "app": "http://localhost:8000",
///       "addon": "addon_2",
///       "name": "ext_2",
///       "extension_group": "another_group"
///     }
///   ]
/// }
///
///   The state will be `UniformDeclared`.
///
/// - Case 3: all nodes have declared the 'app' field, but they have different
///   values.
///
/// {
///   "nodes": [
///     {
///       "type": "extension",
///       "app": "http://localhost:8000",
///       "addon": "addon_1",
///       "name": "ext_1",
///       "extension_group": "some_group"
///     },
///     {
///       "type": "extension",
///       "app": "msgpack://localhost:8001",
///       "addon": "addon_2",
///       "name": "ext_2",
///       "extension_group": "another_group"
///     }
///   ]
/// }
///
///   The state will be `MixedDeclared`.
///
/// - Case 4: some nodes have declared the 'app' field, and some have not. It's
///   illegal.
///
/// In the view of the 'app' field declaration, there are two types of graphs:
///
/// * Single-app graph: the state is `NoneDeclared` or `UniformDeclared`.
/// * Multi-app graph: the state is `MixedDeclared`.
#[derive(Debug, Clone, PartialEq, Eq, Copy)]
pub enum AppUriDeclarationState {
    /// No node has declared an app URI.
    NoneDeclared,
    /// All nodes have declared the same app URI.
    UniformDeclared,
    /// Nodes have declared different app URIs.
    MixedDeclared,
}

impl AppUriDeclarationState {
    /// Returns true if the graph can be considered a single-app graph.
    pub fn is_single_app_graph(&self) -> bool {
        matches!(
            self,
            AppUriDeclarationState::NoneDeclared
                | AppUriDeclarationState::UniformDeclared
        )
    }
}

/// The type of exposed message interface.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
#[serde(rename_all = "snake_case")]
pub enum GraphExposedMessageType {
    CmdIn,
    CmdOut,
    DataIn,
    DataOut,
    AudioFrameIn,
    AudioFrameOut,
    VideoFrameIn,
    VideoFrameOut,
}

/// Represents a message interface that is exposed by the graph to the outside.
/// This is mainly used by development tools to provide intelligent prompts.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphExposedMessage {
    #[serde(rename = "type")]
    pub msg_type: GraphExposedMessageType,

    /// The name of the message.
    /// Must match the regular expression ^[A-Za-z_][A-Za-z0-9_]*$
    pub name: String,

    /// The name of the extension.
    /// Must match the regular expression ^[A-Za-z_][A-Za-z0-9_]*$
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension: Option<String>,

    /// The name of the subgraph.
    /// Must match the regular expression ^[A-Za-z_][A-Za-z0-9_]*$
    #[serde(skip_serializing_if = "Option::is_none")]
    pub subgraph: Option<String>,
}

/// Represents a property that is exposed by the graph to the outside.
/// This is mainly used by development tools to provide intelligent prompts.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphExposedProperty {
    /// The name of the extension.
    /// Must match the regular expression ^[A-Za-z_][A-Za-z0-9_]*$
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension: Option<String>,

    /// The name of the subgraph.
    /// Must match the regular expression ^[A-Za-z_][A-Za-z0-9_]*$
    #[serde(skip_serializing_if = "Option::is_none")]
    pub subgraph: Option<String>,

    /// The name of the property.
    /// Must match the regular expression ^[A-Za-z_][A-Za-z0-9_]*$
    pub name: String,
}

/// Represents a connection graph that defines how extensions connect to each
/// other.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Graph {
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub nodes: Vec<GraphNode>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub connections: Option<Vec<GraphConnection>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub exposed_messages: Option<Vec<GraphExposedMessage>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub exposed_properties: Option<Vec<GraphExposedProperty>>,
}

impl Graph {
    /// Parses a JSON string into a Graph with validation, completion, and
    /// flattening.
    ///
    /// This function takes a JSON string representation of a graph and an
    /// optional current_base_dir parameter, parses it into a Graph structure,
    /// then validates, completes, and flattens the graph.
    ///
    /// # Parameters
    /// - `s`: A string slice containing the JSON representation of the graph
    /// - `current_base_dir`: An optional base directory path used for resolving
    ///   relative paths during graph flattening
    ///
    /// # Returns
    /// - `Ok(Graph)`: Successfully parsed and processed graph
    /// - `Err(anyhow::Error)`: Parsing, validation, or processing error
    pub async fn from_str_with_base_dir(
        s: &str,
        current_base_dir: Option<&str>,
    ) -> Result<Self> {
        let mut graph: Graph = serde_json::from_str(s)?;

        graph.validate_and_complete_and_flatten(current_base_dir).await?;

        // Return the parsed data.
        Ok(graph)
    }

    pub fn from_str_and_validate(s: &str) -> Result<Self> {
        let mut graph: Graph = serde_json::from_str(s)?;
        graph.validate_and_complete(None)?;
        Ok(graph)
    }

    /// Determines how app URIs are declared across all nodes in the graph.
    ///
    /// This method analyzes all nodes in the graph to determine the app
    /// declaration state:
    /// - If no nodes have an 'app' field declared, returns `NoneDeclared`.
    /// - If all nodes have the same 'app' URI declared, returns
    ///   `UniformDeclared`.
    /// - If all nodes have 'app' fields but with different URIs, returns
    ///   `MixedDeclared`.
    /// - If some nodes have 'app' fields and others don't, returns an error as
    ///   this is invalid.
    ///
    /// Graphs can be categorized based on the number of apps:
    /// - A graph for a single app (NoneDeclared or UniformDeclared state)
    /// - A graph spanning multiple apps (MixedDeclared state)
    ///
    /// For a valid graph, either all nodes must have the app field defined or
    /// none of them should. If some nodes have the app field defined while
    /// others do not, it creates an invalid graph because TEN cannot
    /// determine which app the nodes without the defined field belong to.
    /// Therefore, the only valid case where nodes don't define the app
    /// field is when all nodes in the graph lack this field.
    ///
    /// For graphs spanning multiple apps, no node can have 'localhost' as its
    /// app field value, as other apps would not know how to connect to the
    /// app that node belongs to. For consistency, single app graphs also do
    /// not allow 'localhost' as an explicit app field value. Instead,
    /// 'localhost' is used as the internal default value when no app field is
    /// specified.
    fn analyze_app_uri_declaration_state(
        &self,
    ) -> Result<AppUriDeclarationState> {
        let mut nodes_have_declared_app = 0;
        let mut app_uris = std::collections::HashSet::new();

        for (idx, node) in self.nodes.iter().enumerate() {
            if let Some(app_uri) = &node.get_app_uri() {
                if app_uri.is_empty() {
                    return Err(anyhow::anyhow!(
                        "nodes[{}]: {}",
                        idx,
                        ERR_MSG_GRAPH_APP_FIELD_EMPTY
                    ));
                }

                app_uris.insert(app_uri);
                nodes_have_declared_app += 1;
            }
        }

        let extension_nodes_len = self
            .nodes
            .iter()
            .filter(|node| node.get_type() == GraphNodeType::Extension)
            .count();

        // Some nodes have 'app' declared and some don't - this is invalid.
        // Because TEN can not determine which app the nodes without the defined
        // field belong to.
        if nodes_have_declared_app != 0
            && nodes_have_declared_app != extension_nodes_len
        {
            return Err(anyhow::anyhow!(ERR_MSG_GRAPH_MIXED_APP_DECLARATIONS));
        }

        match app_uris.len() {
            // No nodes have 'app' declared.
            0 => Ok(AppUriDeclarationState::NoneDeclared),

            // All nodes have the same 'app' URI declared.
            1 => Ok(AppUriDeclarationState::UniformDeclared),

            // All nodes have 'app' declared but with different URIs.
            _ => Ok(AppUriDeclarationState::MixedDeclared),
        }
    }

    /// Validates and completes the graph by ensuring all nodes and connections
    /// follow the app declaration rules and other validation requirements.
    fn validate_and_complete(
        &mut self,
        _current_base_dir: Option<&str>,
    ) -> Result<()> {
        // Determine the app URI declaration state by examining all nodes.
        let app_uri_declaration_state =
            self.analyze_app_uri_declaration_state()?;

        // Validate all nodes.
        for (idx, node) in self.nodes.iter_mut().enumerate() {
            node.validate_and_complete(&app_uri_declaration_state)
                .map_err(|e| anyhow::anyhow!("nodes[{}]: {}", idx, e))?;
        }

        // Validate all connections if they exist.
        if let Some(connections) = &mut self.connections {
            for (idx, connection) in connections.iter_mut().enumerate() {
                connection
                    .validate_and_complete(&app_uri_declaration_state)
                    .map_err(|e| {
                        anyhow::anyhow!("connections[{}]: {}", idx, e)
                    })?;
            }
        }

        // Validate exposed_properties if they exist
        if let Some(exposed_properties) = &self.exposed_properties {
            for (idx, property) in exposed_properties.iter().enumerate() {
                // Verify that the extension exists in the graph
                if !self.nodes.iter().any(|node| {
                    if let Some(ext) = &property.extension {
                        node.get_name() == ext
                    } else {
                        false
                    }
                }) {
                    return Err(anyhow::anyhow!(
                        "exposed_properties[{}]: extension '{}' does not \
                         exist in the graph",
                        idx,
                        property.extension.as_ref().unwrap_or(&String::new())
                    ));
                }
            }
        }

        Ok(())
    }

    pub async fn validate_and_complete_and_flatten(
        &mut self,
        current_base_dir: Option<&str>,
    ) -> Result<()> {
        // Step 1: Initial validation and completion
        self.validate_and_complete(current_base_dir)?;

        // Step 2: Attempt to flatten the graph
        // Always attempt to flatten the graph, regardless of current_base_dir
        // If there are subgraphs that need current_base_dir but it's None,
        // the flatten_graph method will return an appropriate error.
        if let Some(flattened) = self.flatten_graph(current_base_dir).await? {
            // Replace current graph with flattened version
            *self = flattened;
        }

        // Step 3: Final validation after flattening
        // After flattening, there should basically be no logic that requires
        // current_base_dir, so passing None here should not cause
        // errors, and we can use this for validation.
        self.validate_and_complete(None)?;

        Ok(())
    }

    pub fn check(
        &self,
        graph_app_base_dir: &Option<String>,
        pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    ) -> Result<()> {
        self.static_check()?;

        self.check_nodes_installation(graph_app_base_dir, pkgs_cache, false)?;
        self.check_connections_compatibility(
            graph_app_base_dir,
            pkgs_cache,
            false,
        )?;

        Ok(())
    }

    pub fn check_for_single_app(
        &self,
        graph_app_base_dir: &Option<String>,
        pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    ) -> Result<()> {
        assert!(pkgs_cache.len() == 1);

        self.static_check()?;

        // In a single app, there is no information about pkg_info of other
        // apps, neither the message schemas.
        self.check_nodes_installation(graph_app_base_dir, pkgs_cache, true)?;
        self.check_connections_compatibility(
            graph_app_base_dir,
            pkgs_cache,
            true,
        )?;

        Ok(())
    }

    pub fn static_check(&self) -> Result<()> {
        self.check_extension_uniqueness()?;
        self.check_extension_existence()?;
        self.check_connection_extensions_exist()?;
        self.check_subgraph_references_exist()?;
        self.check_extension_uniqueness_in_connections()?;
        self.check_message_names()?;
        self.check_msg_conversions()?;

        Ok(())
    }

    pub fn get_addon_name_of_extension(
        &self,
        app: &Option<String>,
        extension: &String,
    ) -> Result<&String> {
        self.nodes
            .iter()
            .find(|node| {
                node.get_type() == GraphNodeType::Extension
                    && node.get_name() == extension
                    && node.get_app_uri() == app
            })
            .and_then(|node| {
                if let GraphNode::Extension { content } = node {
                    Some(&content.addon)
                } else {
                    None
                }
            })
            .ok_or_else(|| {
                anyhow::anyhow!(
                    "Extension '{}' is not found in nodes, should not happen.",
                    extension
                )
            })
    }

    /// Convenience method for flattening a graph instance without preserving
    /// exposed info. This is the main public API for flattening graphs.
    ///
    /// Returns `Ok(None)` if the graph doesn't need flattening. Returns
    /// `Ok(Some(flattened_graph))` if the graph was successfully flattened.
    pub async fn flatten_graph(
        &self,
        current_base_dir: Option<&str>,
    ) -> Result<Option<Graph>> {
        let mut processing_graph = self;

        // Step 1: Match nodes according to selector rules and replace them in
        // connections
        let flattened_selector_graph = processing_graph.flatten_selectors()?;
        processing_graph =
            flattened_selector_graph.as_ref().unwrap_or(processing_graph);

        // Step 2: Convert reversed connections to forward connections if needed
        let reversed_graph = processing_graph
            .convert_reversed_connections_to_forward_connections()?;
        processing_graph = reversed_graph.as_ref().unwrap_or(processing_graph);

        // Step 3: Flatten subgraphs
        let flattened =
            Self::flatten_subgraphs(processing_graph, current_base_dir, false)
                .await
                .map_err(|e| {
                    anyhow::anyhow!("Failed to flatten graph: {}", e)
                })?;
        processing_graph = flattened.as_ref().unwrap_or(processing_graph);

        // Check if the processing graph is the same as the original graph.
        if std::ptr::eq(processing_graph, self) {
            return Ok(None);
        }

        Ok(Some(processing_graph.clone()))
    }
}

/// Checks if the application URI is either not specified (None) or set to the
/// default localhost value.
///
/// This function is used to determine if an application's URI is using the
/// default location, which helps decide whether the URI field should be
/// included when serializing property data.
pub fn is_app_default_loc_or_none(app_uri: &Option<String>) -> bool {
    match app_uri {
        None => true,
        Some(uri) => uri == localhost(),
    }
}
