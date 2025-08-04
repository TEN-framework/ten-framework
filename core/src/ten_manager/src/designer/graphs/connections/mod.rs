//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod add;
pub mod delete;
pub mod get;
pub mod msg_conversion;

use serde::{Deserialize, Serialize};

use ten_rust::graph::connection::{
    GraphDestination, GraphLoc, GraphMessageFlow,
};
use ten_rust::graph::msg_conversion::MsgAndResultConversion;

impl From<DesignerMessageFlow> for GraphMessageFlow {
    fn from(designer_msg_flow: DesignerMessageFlow) -> Self {
        GraphMessageFlow::new(
            designer_msg_flow.name,
            designer_msg_flow.dest.into_iter().map(|d| d.into()).collect(),
            vec![],
        )
    }
}

impl From<DesignerDestination> for GraphDestination {
    fn from(designer_destination: DesignerDestination) -> Self {
        GraphDestination {
            loc: GraphLoc {
                app: designer_destination.app,
                extension: Some(designer_destination.extension),
                subgraph: None,
                selector: None,
            },
            msg_conversion: designer_destination.msg_conversion,
        }
    }
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerMessageFlow {
    pub name: String,
    pub dest: Vec<DesignerDestination>,
}

impl From<GraphMessageFlow> for DesignerMessageFlow {
    fn from(msg_flow: GraphMessageFlow) -> Self {
        DesignerMessageFlow {
            name: msg_flow.name.expect(
                "name field should be Some when converting to \
                 DesignerMessageFlow",
            ),
            dest: get_designer_destination_from_property(msg_flow.dest),
        }
    }
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerDestination {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app: Option<String>,

    pub extension: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub msg_conversion: Option<MsgAndResultConversion>,
}

impl From<GraphDestination> for DesignerDestination {
    fn from(destination: GraphDestination) -> Self {
        DesignerDestination {
            app: destination.loc.app,
            extension: destination.loc.extension.unwrap_or_default(),
            msg_conversion: destination.msg_conversion,
        }
    }
}

fn get_designer_destination_from_property(
    destinations: Vec<GraphDestination>,
) -> Vec<DesignerDestination> {
    destinations.into_iter().map(|v| v.into()).collect()
}
