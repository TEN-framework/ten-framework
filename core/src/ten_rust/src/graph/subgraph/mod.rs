//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod flatten;

use anyhow::Result;

use crate::{
    graph::{
        connection::GraphLoc,
        graph_info::load_graph_from_uri,
        node::{ExtensionNode, GraphNode},
        Graph, GraphExposedMessageType, GraphNodeType,
    },
    pkg_info::message::{MsgDirection, MsgType},
    utils::path::{get_base_dir_of_uri, get_real_path_from_import_uri},
};

impl Graph {
    /// Helper function to resolve subgraph reference to actual extension name.
    /// This function looks up the exposed_messages in the subgraph to find
    /// the corresponding extension for a given message flow.
    fn resolve_subgraph_to_extension(
        subgraph_name: &str,
        msg_name: &str,
        msg_type: GraphExposedMessageType,
        subgraph: &Graph,
    ) -> Result<String> {
        if let Some(exposed_messages) = &subgraph.exposed_messages {
            let matching_exposed = exposed_messages
                .iter()
                .find(|exposed| exposed.msg_type == msg_type && exposed.name == msg_name);

            if let Some(exposed) = matching_exposed {
                if let Some(ref extension_name) = exposed.extension {
                    Ok(extension_name.clone())
                } else {
                    Err(anyhow::anyhow!(
                        "Exposed message '{}' in subgraph '{}' does not specify an extension",
                        msg_name,
                        subgraph_name
                    ))
                }
            } else {
                Err(anyhow::anyhow!(
                    "Message '{}' of type '{:?}' is not exposed by subgraph '{}'",
                    msg_name,
                    msg_type,
                    subgraph_name
                ))
            }
        } else {
            Err(anyhow::anyhow!(
                "Subgraph '{}' does not have exposed_messages defined",
                subgraph_name
            ))
        }
    }

    /// Helper function to determine the appropriate GraphExposedMessageType
    /// based on message type string.
    fn get_exposed_message_type_for_dest_subgraph(
        msg_type: &str,
    ) -> Result<GraphExposedMessageType> {
        match msg_type {
            "cmd" => Ok(GraphExposedMessageType::CmdIn),
            "data" => Ok(GraphExposedMessageType::DataIn),
            "audio_frame" => Ok(GraphExposedMessageType::AudioFrameIn),
            "video_frame" => Ok(GraphExposedMessageType::VideoFrameIn),
            _ => Err(anyhow::anyhow!("Unknown message type: {}", msg_type)),
        }
    }

    /// Helper function to get addon name for both extension and subgraph nodes
    pub async fn get_addon_name_of_node(
        &self,
        base_dir: &Option<String>,
        loc: &GraphLoc,
        msg_type: &crate::pkg_info::message::MsgType,
        msg_name: &str,
        msg_direction: MsgDirection,
    ) -> Result<String> {
        match loc.get_node_type()? {
            GraphNodeType::Extension => {
                let extension_name = loc.extension.as_ref().unwrap();
                Ok(self.get_addon_name_of_extension(&loc.app, extension_name)?.clone())
            }
            GraphNodeType::Subgraph => {
                let subgraph_name = loc.subgraph.as_ref().unwrap();
                let extension_node = self
                    .get_extension_node_from_subgraph_using_exposed_message(
                        base_dir,
                        subgraph_name,
                        msg_type,
                        msg_name,
                        msg_direction,
                    )
                    .await?;
                Ok(extension_node.addon)
            }
            GraphNodeType::Selector => Err(anyhow::anyhow!(
                "Selector node are not supported in get_addon_name_of_node, it should be broken \
                 down into extension nodes."
            )),
        }
    }

    /// Recursively finds an extension node from a subgraph using exposed
    /// message. This function handles nested subgraphs by recursively
    /// searching until it finds the actual extension node, not another
    /// subgraph.
    pub async fn get_extension_node_from_subgraph_using_exposed_message(
        &self,
        base_dir: &Option<String>,
        subgraph_name: &str,
        msg_type: &MsgType,
        msg_name: &str,
        msg_direction: MsgDirection,
    ) -> Result<ExtensionNode> {
        // Find the subgraph node
        let subgraph_node = self
            .nodes
            .iter()
            .find(|node| {
                node.get_type() == GraphNodeType::Subgraph && node.get_name() == subgraph_name
            })
            .ok_or_else(|| {
                anyhow::anyhow!(
                    "Subgraph '{}' is not found in nodes, should not happen.",
                    subgraph_name
                )
            })?;

        // Get the subgraph content
        let subgraph_content = if let GraphNode::Subgraph {
            content,
        } = subgraph_node
        {
            content
        } else {
            return Err(anyhow::anyhow!(
                "Node '{}' is not a subgraph node, should not happen.",
                subgraph_name
            ));
        };

        // Convert MsgType to GraphExposedMessageType
        let exposed_msg_type = if msg_direction == MsgDirection::Out {
            match msg_type {
                MsgType::Cmd => GraphExposedMessageType::CmdOut,
                MsgType::Data => GraphExposedMessageType::DataOut,
                MsgType::AudioFrame => GraphExposedMessageType::AudioFrameOut,
                MsgType::VideoFrame => GraphExposedMessageType::VideoFrameOut,
            }
        } else {
            match msg_type {
                MsgType::Cmd => GraphExposedMessageType::CmdIn,
                MsgType::Data => GraphExposedMessageType::DataIn,
                MsgType::AudioFrame => GraphExposedMessageType::AudioFrameIn,
                MsgType::VideoFrame => GraphExposedMessageType::VideoFrameIn,
            }
        };

        // Load the subgraph from the import_uri
        let subgraph_graph =
            load_graph_from_uri(&subgraph_content.graph.import_uri, base_dir.as_deref(), &mut None)
                .await
                .map_err(|e| {
                    anyhow::anyhow!("Failed to load subgraph '{}': {}", subgraph_name, e)
                })?;

        // Find the extension specified by the exposed message
        let extension_name = Self::resolve_subgraph_to_extension(
            subgraph_name,
            msg_name,
            exposed_msg_type,
            &subgraph_graph,
        )?;

        // Try to find the extension node directly in the current subgraph
        if let Some(GraphNode::Extension {
            content,
        }) = subgraph_graph.nodes.iter().find(|node| {
            if let GraphNode::Extension {
                content,
            } = node
            {
                content.name == extension_name
            } else {
                false
            }
        }) {
            return Ok(content.clone());
        }

        // If not found as extension, check if it's a subgraph and recurse
        for node in &subgraph_graph.nodes {
            if let GraphNode::Subgraph {
                content,
            } = node
            {
                let real_path = get_real_path_from_import_uri(
                    &subgraph_content.graph.import_uri,
                    base_dir.as_deref(),
                    None,
                )?;
                let nested_base_dir = Some(get_base_dir_of_uri(&real_path)?);
                return Box::pin(
                    subgraph_graph.get_extension_node_from_subgraph_using_exposed_message(
                        &nested_base_dir,
                        &content.name,
                        msg_type,
                        msg_name,
                        msg_direction,
                    ),
                )
                .await;
            }
        }

        // If neither extension nor subgraph found, return error
        Err(anyhow::anyhow!(
            "Extension or subgraph '{}' not found in subgraph '{}'",
            extension_name,
            subgraph_name
        ))
    }
}
