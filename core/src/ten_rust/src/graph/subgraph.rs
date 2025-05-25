//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::Result;

use super::{Graph, GraphConnection, GraphMessageFlow, GraphNodeType};

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
                Self::update_connection_source(&mut flattened_connection);

                // Update all message flow destinations
                Self::update_message_flows_for_flattening(
                    &mut flattened_connection,
                );

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
    /// colon notation (e.g., "subgraph_1:ext_c" -> "subgraph_1_ext_c").
    fn update_connection_source(connection: &mut GraphConnection) {
        if let Some(ref extension) = connection.loc.extension {
            if extension.contains(':') {
                let parts: Vec<&str> = extension.split(':').collect();
                if parts.len() == 2 {
                    connection.loc.extension =
                        Some(format!("{}_{}", parts[0], parts[1]));
                }
            }
        }
    }

    /// Updates all message flow destinations to convert subgraph references
    /// from colon notation to underscore notation.
    fn update_message_flows_for_flattening(connection: &mut GraphConnection) {
        let update_destinations = |flows: &mut Vec<GraphMessageFlow>| {
            for flow in flows {
                for dest in &mut flow.dest {
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
}
