//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::Result;

use super::{
    Graph, GraphConnection, GraphExposedMessageType, GraphMessageFlow,
    GraphNodeType,
};

impl Graph {
    /// Flattens a graph containing subgraph nodes into a regular graph
    /// structure with only extension nodes. This process converts subgraph
    /// references into their constituent extensions with prefixed names and
    /// merges all connections.
    pub fn flatten<F>(&self, subgraph_loader: F) -> Result<Graph>
    where
        F: Fn(&str) -> Result<Graph>,
    {
        let mut flattened_nodes = Vec::new();
        let mut flattened_connections = Vec::new();

        // Keep track of subgraph mappings for connection resolution
        let mut subgraph_mappings: HashMap<String, Graph> = HashMap::new();

        // Process all nodes
        for node in &self.nodes {
            match node.type_ {
                GraphNodeType::Extension => {
                    // Extension nodes are kept as-is
                    flattened_nodes.push(node.clone());
                }
                GraphNodeType::Subgraph => {
                    // Load subgraph content
                    let source_uri =
                        node.source_uri.as_ref().ok_or_else(|| {
                            anyhow::anyhow!(
                                "Subgraph node '{}' must have source_uri",
                                node.name
                            )
                        })?;

                    let subgraph = subgraph_loader(source_uri)?;
                    subgraph_mappings
                        .insert(node.name.clone(), subgraph.clone());

                    // Flatten subgraph nodes
                    for sub_node in &subgraph.nodes {
                        if sub_node.type_ != GraphNodeType::Extension {
                            // TODO(Wei): Support nested subgraphs
                            return Err(anyhow::anyhow!(
                                "Nested subgraphs are not supported in \
                                 subgraph '{}'",
                                node.name
                            ));
                        }

                        let mut flattened_node = sub_node.clone();
                        // Add subgraph name as prefix
                        flattened_node.name =
                            format!("{}_{}", node.name, sub_node.name);

                        // Merge properties if specified in the subgraph
                        // reference
                        if let Some(ref_property) = &node.property {
                            match (&mut flattened_node.property, ref_property) {
                                (Some(node_prop), ref_prop) => {
                                    // Merge properties - reference properties
                                    // override node properties
                                    if let (
                                        serde_json::Value::Object(node_obj),
                                        serde_json::Value::Object(ref_obj),
                                    ) = (node_prop, ref_prop)
                                    {
                                        for (key, value) in ref_obj {
                                            node_obj.insert(
                                                key.clone(),
                                                value.clone(),
                                            );
                                        }
                                    }
                                }
                                (None, ref_prop) => {
                                    flattened_node.property =
                                        Some(ref_prop.clone());
                                }
                            }
                        }

                        flattened_nodes.push(flattened_node);
                    }

                    // Add internal connections from subgraph
                    if let Some(sub_connections) = &subgraph.connections {
                        for connection in sub_connections {
                            let mut flattened_connection = connection.clone();

                            // Update extension names in the connection source
                            if let Some(ref extension) =
                                flattened_connection.loc.extension
                            {
                                flattened_connection.loc.extension = Some(
                                    format!("{}_{}", node.name, extension),
                                );
                            }

                            // Update extension names in all message flows
                            Self::update_message_flows_for_subgraph(
                                &mut flattened_connection,
                                &node.name,
                            );

                            flattened_connections.push(flattened_connection);
                        }
                    }
                }
            }
        }

        // Process connections from the main graph
        if let Some(connections) = &self.connections {
            for connection in connections {
                let mut flattened_connection = connection.clone();

                // Update connection source if it references a subgraph element
                Self::update_connection_source(
                    &mut flattened_connection,
                    &subgraph_mappings,
                )?;

                // Update all message flow destinations
                Self::update_message_flows_for_flattening(
                    &mut flattened_connection,
                    &subgraph_mappings,
                )?;

                flattened_connections.push(flattened_connection);
            }
        }

        Ok(Graph {
            nodes: flattened_nodes,
            connections: if flattened_connections.is_empty() {
                None
            } else {
                Some(flattened_connections)
            },
            // exposed_messages and exposed_properties are discarded during
            // flattening
            exposed_messages: None,
            exposed_properties: None,
        })
    }

    /// Updates message flows within a connection to use flattened names for
    /// subgraph elements.
    fn update_message_flows_for_subgraph(
        connection: &mut GraphConnection,
        subgraph_name: &str,
    ) {
        let update_destinations = |flows: &mut Vec<GraphMessageFlow>| {
            for flow in flows {
                for dest in &mut flow.dest {
                    if let Some(ref extension) = dest.loc.extension {
                        dest.loc.extension =
                            Some(format!("{}_{}", subgraph_name, extension));
                    }
                    if let Some(ref subgraph) = dest.loc.subgraph {
                        dest.loc.subgraph =
                            Some(format!("{}_{}", subgraph_name, subgraph));
                    }
                }
            }
        };

        if let Some(ref mut cmd) = connection.cmd {
            update_destinations(cmd);
        }
        if let Some(ref mut data) = connection.data {
            update_destinations(data);
        }
        if let Some(ref mut audio_frame) = connection.audio_frame {
            update_destinations(audio_frame);
        }
        if let Some(ref mut video_frame) = connection.video_frame {
            update_destinations(video_frame);
        }
    }

    /// Updates connection source if it references a subgraph element using
    /// colon notation (e.g., "subgraph_1:ext_c" -> "subgraph_1_ext_c") or
    /// subgraph field.
    fn update_connection_source(
        connection: &mut GraphConnection,
        subgraph_mappings: &HashMap<String, Graph>,
    ) -> Result<()> {
        // Handle colon notation in extension field
        if let Some(ref extension) = connection.loc.extension {
            if extension.contains(':') {
                let parts: Vec<&str> = extension.split(':').collect();
                if parts.len() == 2 {
                    connection.loc.extension =
                        Some(format!("{}_{}", parts[0], parts[1]));
                }
            }
        }

        // Handle subgraph field - resolve to actual extension based on
        // exposed_messages
        if let Some(ref subgraph_name) = connection.loc.subgraph.clone() {
            let subgraph =
                subgraph_mappings.get(subgraph_name).ok_or_else(|| {
                    anyhow::anyhow!("Subgraph '{}' not found", subgraph_name)
                })?;

            // Helper function to process message flows for a specific type
            let process_flows = |flows: &mut Vec<GraphMessageFlow>,
                                 msg_type: GraphExposedMessageType|
             -> Result<Option<String>> {
                if let Some(flow) = flows.first() {
                    if let Some(exposed_messages) = &subgraph.exposed_messages {
                        let matching_exposed =
                            exposed_messages.iter().find(|exposed| {
                                exposed.msg_type == msg_type
                                    && exposed.name == flow.name
                            });

                        if let Some(exposed) = matching_exposed {
                            if let Some(ref extension_name) = exposed.extension
                            {
                                return Ok(Some(format!(
                                    "{}_{}",
                                    subgraph_name, extension_name
                                )));
                            } else {
                                return Err(anyhow::anyhow!(
                                    "Exposed message '{}' in subgraph '{}' \
                                     does not specify an extension",
                                    flow.name,
                                    subgraph_name
                                ));
                            }
                        } else {
                            return Err(anyhow::anyhow!(
                                "Message '{}' of type '{:?}' is not exposed \
                                 by subgraph '{}'",
                                flow.name,
                                msg_type,
                                subgraph_name
                            ));
                        }
                    } else {
                        return Err(anyhow::anyhow!(
                            "Subgraph '{}' does not have exposed_messages \
                             defined",
                            subgraph_name
                        ));
                    }
                }
                Ok(None)
            };

            // Process each message type
            if let Some(ref mut cmd) = connection.cmd {
                if let Some(extension_name) =
                    process_flows(cmd, GraphExposedMessageType::CmdOut)?
                {
                    connection.loc.extension = Some(extension_name);
                }
            }

            if let Some(ref mut data) = connection.data {
                if let Some(extension_name) =
                    process_flows(data, GraphExposedMessageType::DataOut)?
                {
                    connection.loc.extension = Some(extension_name);
                }
            }

            if let Some(ref mut audio_frame) = connection.audio_frame {
                if let Some(extension_name) = process_flows(
                    audio_frame,
                    GraphExposedMessageType::AudioFrameOut,
                )? {
                    connection.loc.extension = Some(extension_name);
                }
            }

            if let Some(ref mut video_frame) = connection.video_frame {
                if let Some(extension_name) = process_flows(
                    video_frame,
                    GraphExposedMessageType::VideoFrameOut,
                )? {
                    connection.loc.extension = Some(extension_name);
                }
            }

            // Clear the subgraph field after processing
            connection.loc.subgraph = None;
        }

        Ok(())
    }

    /// Updates all message flow destinations to convert subgraph references
    /// from colon notation to underscore notation and resolve subgraph field
    /// references using exposed_messages.
    fn update_message_flows_for_flattening(
        connection: &mut GraphConnection,
        subgraph_mappings: &HashMap<String, Graph>,
    ) -> Result<()> {
        let update_destinations = |flows: &mut Vec<GraphMessageFlow>,
                                   msg_type: &str,
                                   is_source: bool|
         -> Result<()> {
            for flow in flows {
                for dest in &mut flow.dest {
                    // Handle colon notation in extension field
                    if let Some(ref extension) = dest.loc.extension {
                        if extension.contains(':') {
                            let parts: Vec<&str> =
                                extension.split(':').collect();
                            if parts.len() == 2 {
                                dest.loc.extension =
                                    Some(format!("{}_{}", parts[0], parts[1]));
                            }
                        }
                    }

                    // Handle subgraph field - resolve to actual extension based
                    // on exposed_messages
                    if let Some(ref subgraph_name) = dest.loc.subgraph {
                        let subgraph = subgraph_mappings
                            .get(subgraph_name)
                            .ok_or_else(|| {
                                anyhow::anyhow!(
                                    "Subgraph '{}' not found",
                                    subgraph_name
                                )
                            })?;

                        // Determine the message type to look for in
                        // exposed_messages
                        let exposed_msg_type = match (msg_type, is_source) {
                            ("cmd", false) => GraphExposedMessageType::CmdIn,
                            ("cmd", true) => GraphExposedMessageType::CmdOut,
                            ("data", false) => GraphExposedMessageType::DataIn,
                            ("data", true) => GraphExposedMessageType::DataOut,
                            ("audio_frame", false) => {
                                GraphExposedMessageType::AudioFrameIn
                            }
                            ("audio_frame", true) => {
                                GraphExposedMessageType::AudioFrameOut
                            }
                            ("video_frame", false) => {
                                GraphExposedMessageType::VideoFrameIn
                            }
                            ("video_frame", true) => {
                                GraphExposedMessageType::VideoFrameOut
                            }
                            _ => {
                                return Err(anyhow::anyhow!(
                                    "Unknown message type: {}",
                                    msg_type
                                ))
                            }
                        };

                        // Find the corresponding extension in exposed_messages
                        if let Some(exposed_messages) =
                            &subgraph.exposed_messages
                        {
                            let matching_exposed =
                                exposed_messages.iter().find(|exposed| {
                                    exposed.msg_type == exposed_msg_type
                                        && exposed.name == flow.name
                                });

                            if let Some(exposed) = matching_exposed {
                                if let Some(ref extension_name) =
                                    exposed.extension
                                {
                                    // Replace subgraph reference with the
                                    // actual extension
                                    dest.loc.extension = Some(format!(
                                        "{}_{}",
                                        subgraph_name, extension_name
                                    ));
                                    dest.loc.subgraph = None;
                                } else {
                                    return Err(anyhow::anyhow!(
                                        "Exposed message '{}' in subgraph \
                                         '{}' does not specify an extension",
                                        flow.name,
                                        subgraph_name
                                    ));
                                }
                            } else {
                                return Err(anyhow::anyhow!(
                                    "Message '{}' of type '{:?}' is not \
                                     exposed by subgraph '{}'",
                                    flow.name,
                                    exposed_msg_type,
                                    subgraph_name
                                ));
                            }
                        } else {
                            return Err(anyhow::anyhow!(
                                "Subgraph '{}' does not have exposed_messages \
                                 defined",
                                subgraph_name
                            ));
                        }
                    }

                    // Handle colon notation in subgraph field (for nested
                    // references)
                    if let Some(ref subgraph) = dest.loc.subgraph {
                        if subgraph.contains(':') {
                            let parts: Vec<&str> =
                                subgraph.split(':').collect();
                            if parts.len() == 2 {
                                dest.loc.subgraph =
                                    Some(format!("{}_{}", parts[0], parts[1]));
                            }
                        }
                    }
                }
            }
            Ok(())
        };

        if let Some(ref mut cmd) = connection.cmd {
            update_destinations(cmd, "cmd", false)?;
        }
        if let Some(ref mut data) = connection.data {
            update_destinations(data, "data", false)?;
        }
        if let Some(ref mut audio_frame) = connection.audio_frame {
            update_destinations(audio_frame, "audio_frame", false)?;
        }
        if let Some(ref mut video_frame) = connection.video_frame {
            update_destinations(video_frame, "video_frame", false)?;
        }

        Ok(())
    }
}
