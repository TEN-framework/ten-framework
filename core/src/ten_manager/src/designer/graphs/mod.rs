//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod auto_start;
pub mod connections;
pub mod get;
pub mod nodes;
pub mod update;
pub mod util;

use serde::{Deserialize, Serialize};
use ten_rust::graph::GraphExposedMessageType;
use uuid::Uuid;

use crate::designer::graphs::{connections::DesignerGraphConnection, nodes::DesignerGraphNode};

#[derive(Serialize, Deserialize, Debug)]
pub struct DesignerGraphInfo {
    pub graph_id: Uuid,

    pub name: Option<String>,
    pub auto_start: Option<bool>,
    pub base_dir: Option<String>,

    pub graph: DesignerGraph,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct DesignerGraph {
    pub nodes: Vec<DesignerGraphNode>,
    pub connections: Vec<DesignerGraphConnection>,
    pub exposed_messages: Vec<DesignerGraphExposedMessage>,
    pub exposed_properties: Vec<DesignerGraphExposedProperty>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct DesignerGraphExposedMessage {
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

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct DesignerGraphExposedProperty {
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
