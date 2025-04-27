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

use std::{collections::HashMap, str::FromStr};

use anyhow::Result;
use connection::GraphConnection;
use serde::{Deserialize, Serialize};

use crate::{
    base_dir_pkg_info::PkgsInfoInApp,
    constants::{
        ERR_MSG_GRAPH_APP_FIELD_EMPTY, ERR_MSG_GRAPH_MIXED_APP_DECLARATIONS,
    },
    pkg_info::{localhost, pkg_type::PkgType},
};

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

/// Represents a connection graph that defines how extensions connect to each
/// other.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Graph {
    pub nodes: Vec<node::GraphNode>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub connections: Option<Vec<connection::GraphConnection>>,
}

impl FromStr for Graph {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut graph: Graph = serde_json::from_str(s)?;

        graph.validate_and_complete()?;

        // Return the parsed data.
        Ok(graph)
    }
}

impl Graph {
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
            if let Some(app_uri) = &node.app {
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

        // Some nodes have 'app' declared and some don't - this is invalid.
        // Because TEN can not determine which app the nodes without the defined
        // field belong to.
        if nodes_have_declared_app != 0
            && nodes_have_declared_app != self.nodes.len()
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
    pub fn validate_and_complete(&mut self) -> Result<()> {
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

        Ok(())
    }

    pub fn check(
        &self,
        graph_app_base_dir: &Option<String>,
        pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    ) -> Result<()> {
        self.check_extension_uniqueness()?;
        self.check_extension_existence()?;
        self.check_connection_extensions_exist()?;

        self.check_nodes_installation(graph_app_base_dir, pkgs_cache, false)?;
        self.check_connections_compatibility(
            graph_app_base_dir,
            pkgs_cache,
            false,
        )?;

        self.check_extension_uniqueness_in_connections()?;
        self.check_message_names()?;

        Ok(())
    }

    pub fn check_for_single_app(
        &self,
        graph_app_base_dir: &Option<String>,
        pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    ) -> Result<()> {
        assert!(pkgs_cache.len() == 1);

        self.check_extension_uniqueness()?;
        self.check_extension_existence()?;
        self.check_connection_extensions_exist()?;

        // In a single app, there is no information about pkg_info of other
        // apps, neither the message schemas.
        self.check_nodes_installation(graph_app_base_dir, pkgs_cache, true)?;
        self.check_connections_compatibility(
            graph_app_base_dir,
            pkgs_cache,
            true,
        )?;

        self.check_extension_uniqueness_in_connections()?;
        self.check_message_names()?;

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
                node.type_and_name.pkg_type == PkgType::Extension
                    && node.type_and_name.name.as_str() == extension
                    && node.get_app_uri() == app
            })
            .map(|node| &node.addon)
            .ok_or_else(|| {
                anyhow::anyhow!(
                    "Extension '{}' is not found in nodes, should not happen.",
                    extension
                )
            })
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
