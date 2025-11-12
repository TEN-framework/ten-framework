//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod add;
pub mod delete;
pub mod msg_conversion;

use serde::{Deserialize, Serialize};
use ten_rust::graph::{
    connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow, GraphSource},
    msg_conversion::MsgAndResultConversion,
};

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq, Hash)]
pub struct DesignerGraphLoc {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub subgraph: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub selector: Option<String>,
}

impl DesignerGraphLoc {
    pub fn new() -> Self {
        Self {
            app: None,
            extension: None,
            subgraph: None,
            selector: None,
        }
    }

    pub fn with_app_and_extension_or_subgraph(
        app: Option<String>,
        extension: Option<String>,
        subgraph: Option<String>,
    ) -> Self {
        Self {
            app,
            extension,
            subgraph,
            selector: None,
        }
    }
}

impl Default for DesignerGraphLoc {
    fn default() -> Self {
        Self::new()
    }
}

impl From<DesignerGraphMessageFlow> for GraphMessageFlow {
    fn from(designer_msg_flow: DesignerGraphMessageFlow) -> Self {
        GraphMessageFlow {
            name: designer_msg_flow.name,
            names: designer_msg_flow.names,
            dest: designer_msg_flow.dest.into_iter().map(|d| d.into()).collect(),
            source: designer_msg_flow.source.into_iter().map(|s| s.into()).collect(),
        }
    }
}

impl From<DesignerGraphDestination> for GraphDestination {
    fn from(designer_destination: DesignerGraphDestination) -> Self {
        GraphDestination {
            loc: GraphLoc {
                app: designer_destination.loc.app,
                extension: designer_destination.loc.extension,
                subgraph: designer_destination.loc.subgraph,
                selector: designer_destination.loc.selector,
            },
            msg_conversion: designer_destination.msg_conversion,
        }
    }
}

impl From<DesignerGraphSource> for GraphSource {
    fn from(designer_source: DesignerGraphSource) -> Self {
        GraphSource {
            loc: GraphLoc {
                app: designer_source.loc.app,
                extension: designer_source.loc.extension,
                subgraph: designer_source.loc.subgraph,
                selector: designer_source.loc.selector,
            },
        }
    }
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerGraphMessageFlow {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub names: Option<Vec<String>>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dest: Vec<DesignerGraphDestination>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub source: Vec<DesignerGraphSource>,
}

impl From<GraphMessageFlow> for DesignerGraphMessageFlow {
    fn from(msg_flow: GraphMessageFlow) -> Self {
        DesignerGraphMessageFlow {
            name: msg_flow.name,
            names: msg_flow.names,
            dest: get_designer_destination_from_property(msg_flow.dest),
            source: get_designer_source_from_property(msg_flow.source),
        }
    }
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerGraphDestination {
    #[serde(flatten)]
    pub loc: DesignerGraphLoc,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub msg_conversion: Option<MsgAndResultConversion>,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerGraphSource {
    #[serde(flatten)]
    pub loc: DesignerGraphLoc,
}

impl From<GraphDestination> for DesignerGraphDestination {
    fn from(destination: GraphDestination) -> Self {
        DesignerGraphDestination {
            loc: DesignerGraphLoc {
                app: destination.loc.app,
                extension: destination.loc.extension,
                subgraph: destination.loc.subgraph,
                selector: destination.loc.selector,
            },
            msg_conversion: destination.msg_conversion,
        }
    }
}

impl From<GraphSource> for DesignerGraphSource {
    fn from(source: GraphSource) -> Self {
        DesignerGraphSource {
            loc: DesignerGraphLoc {
                app: source.loc.app,
                extension: source.loc.extension,
                subgraph: source.loc.subgraph,
                selector: source.loc.selector,
            },
        }
    }
}

fn get_designer_destination_from_property(
    destinations: Vec<GraphDestination>,
) -> Vec<DesignerGraphDestination> {
    destinations.into_iter().map(|v| v.into()).collect()
}

fn get_designer_source_from_property(sources: Vec<GraphSource>) -> Vec<DesignerGraphSource> {
    sources.into_iter().map(|v| v.into()).collect()
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerGraphConnection {
    #[serde(flatten)]
    pub loc: DesignerGraphLoc,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub cmd: Option<Vec<DesignerGraphMessageFlow>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub data: Option<Vec<DesignerGraphMessageFlow>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_frame: Option<Vec<DesignerGraphMessageFlow>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_frame: Option<Vec<DesignerGraphMessageFlow>>,
}

fn get_designer_msg_flow_from_property(
    msg_flow: Vec<GraphMessageFlow>,
) -> Vec<DesignerGraphMessageFlow> {
    if msg_flow.is_empty() {
        return vec![];
    }

    msg_flow.into_iter().map(|v| v.into()).collect()
}

fn get_property_msg_flow_from_designer(
    msg_flow: Vec<DesignerGraphMessageFlow>,
) -> Vec<GraphMessageFlow> {
    msg_flow.into_iter().map(|v| v.into()).collect()
}

impl From<GraphConnection> for DesignerGraphConnection {
    fn from(conn: GraphConnection) -> Self {
        DesignerGraphConnection {
            loc: DesignerGraphLoc {
                app: conn.loc.app,
                extension: conn.loc.extension,
                subgraph: conn.loc.subgraph,
                selector: conn.loc.selector,
            },

            cmd: conn.cmd.map(get_designer_msg_flow_from_property),

            data: conn.data.map(get_designer_msg_flow_from_property),

            audio_frame: conn.audio_frame.map(get_designer_msg_flow_from_property),

            video_frame: conn.video_frame.map(get_designer_msg_flow_from_property),
        }
    }
}

impl From<DesignerGraphConnection> for GraphConnection {
    fn from(designer_connection: DesignerGraphConnection) -> Self {
        GraphConnection {
            loc: GraphLoc {
                app: designer_connection.loc.app,
                extension: designer_connection.loc.extension,
                subgraph: designer_connection.loc.subgraph,
                selector: designer_connection.loc.selector,
            },

            cmd: designer_connection.cmd.map(get_property_msg_flow_from_designer),
            data: designer_connection.data.map(get_property_msg_flow_from_designer),
            audio_frame: designer_connection.audio_frame.map(get_property_msg_flow_from_designer),
            video_frame: designer_connection.video_frame.map(get_property_msg_flow_from_designer),
        }
    }
}
