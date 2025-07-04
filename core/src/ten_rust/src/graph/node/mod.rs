//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::constants::ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_MULTI_APP_MODE;
use crate::constants::ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_SINGLE_APP_MODE;
use crate::graph::AppUriDeclarationState;

use crate::graph::is_app_default_loc_or_none;
use crate::pkg_info::localhost;

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum GraphNodeType {
    Extension,
    Subgraph,
}

/// Represents an extension node in the graph
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ExtensionNode {
    pub name: String,
    pub addon: String,

    /// The extension group this node belongs to. Extension group nodes
    /// themselves do not contain this field, as they define groups rather
    /// than belong to them.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,

    #[serde(skip_serializing_if = "is_app_default_loc_or_none")]
    pub app: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<serde_json::Value>,
}

/// Represents a subgraph node in the graph
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct SubgraphNode {
    pub name: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<serde_json::Value>,

    pub graph: GraphContent,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphContent {
    pub import_uri: String,
}

/// Represents a node in a graph. This enum represents different types of nodes
/// that can exist in the graph.
#[derive(Serialize, Deserialize, Debug, Clone)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum GraphNode {
    Extension {
        #[serde(flatten)]
        content: ExtensionNode,
    },
    Subgraph {
        #[serde(flatten)]
        content: SubgraphNode,
    },
}

impl GraphNode {
    pub fn new_extension_node(
        name: String,
        addon: String,
        extension_group: Option<String>,
        app: Option<String>,
        property: Option<serde_json::Value>,
    ) -> Self {
        Self::Extension {
            content: ExtensionNode {
                name,
                addon,
                extension_group,
                app,
                property,
            },
        }
    }

    pub fn new_subgraph_node(
        name: String,
        property: Option<serde_json::Value>,
        graph: GraphContent,
    ) -> Self {
        Self::Subgraph { content: SubgraphNode { name, property, graph } }
    }

    /// Validates and completes a graph node by ensuring it has all required
    /// fields and follows the app declaration rules of the graph.
    ///
    /// For graphs spanning multiple apps, no node can have 'localhost' as its
    /// app field value, as other apps would not know how to connect to the
    /// app that node belongs to. For consistency, single app graphs also do
    /// not allow 'localhost' as an explicit app field value. Instead,
    /// 'localhost' is used as the internal default value when no app field is
    /// specified.
    pub fn validate_and_complete(
        &mut self,
        app_uri_declaration_state: &AppUriDeclarationState,
    ) -> Result<()> {
        match self {
            GraphNode::Extension { content } => {
                // Validate app URI if provided
                if let Some(app) = &content.app {
                    // Disallow 'localhost' as an app URI in graph definitions.
                    if app.as_str() == localhost() {
                        let err_msg = if app_uri_declaration_state
                            .is_single_app_graph()
                        {
                            ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_SINGLE_APP_MODE
                        } else {
                            ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_MULTI_APP_MODE
                        };
                        return Err(anyhow::anyhow!(err_msg));
                    }
                }
                Ok(())
            }
            GraphNode::Subgraph { .. } => Ok(()),
        }
    }

    pub fn get_app_uri(&self) -> &Option<String> {
        match self {
            GraphNode::Extension { content } => &content.app,
            GraphNode::Subgraph { .. } => &None,
        }
    }

    pub fn get_type(&self) -> GraphNodeType {
        match self {
            GraphNode::Extension { .. } => GraphNodeType::Extension,
            GraphNode::Subgraph { .. } => GraphNodeType::Subgraph,
        }
    }

    pub fn get_name(&self) -> &str {
        match self {
            GraphNode::Extension { content } => &content.name,
            GraphNode::Subgraph { content } => &content.name,
        }
    }

    pub fn set_name(&mut self, name: String) {
        match self {
            GraphNode::Extension { content } => content.name = name,
            GraphNode::Subgraph { content } => content.name = name,
        }
    }
}
