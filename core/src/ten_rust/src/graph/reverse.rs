//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use crate::graph::{
    connection::{
        GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow,
    },
    Graph,
};
use anyhow::Result;
use std::collections::HashMap;

impl Graph {
    /// Helper function to process a single type of message flows
    fn process_message_flows(
        flows: &[GraphMessageFlow],
        flow_type: &str,
        conn_loc: &GraphDestination,
        new_connections: &mut Vec<GraphConnection>,
    ) -> Option<Vec<GraphMessageFlow>> {
        let mut new_flows = Vec::new();
        let mut has_source = false;

        for flow in flows {
            if flow.source.is_empty() {
                new_flows.push(flow.clone());
                continue;
            }

            has_source = true;
            // Create forward connections for each source
            for src in &flow.source {
                let mut forward_conn = GraphConnection::new(
                    src.loc.app.clone(),
                    src.loc.extension.clone(),
                    src.loc.subgraph.clone(),
                );

                // Create a new message flow with the current destinations
                let mut msg_flows = Vec::new();
                let forward_flow = GraphMessageFlow {
                    name: flow.name.clone(),
                    dest: vec![GraphDestination {
                        loc: conn_loc.loc.clone(),
                        msg_conversion: None,
                    }],
                    source: Vec::new(),
                };
                msg_flows.push(forward_flow);

                // Set the appropriate flow type
                match flow_type {
                    "cmd" => forward_conn.cmd = Some(msg_flows),
                    "data" => forward_conn.data = Some(msg_flows),
                    "audio_frame" => forward_conn.audio_frame = Some(msg_flows),
                    "video_frame" => forward_conn.video_frame = Some(msg_flows),
                    _ => unreachable!(),
                }

                new_connections.push(forward_conn);
            }
        }

        let result = if has_source { new_flows } else { flows.to_vec() };

        if result.is_empty() {
            None
        } else {
            Some(result)
        }
    }

    /// Helper function to convert reversed connections to forward connections.
    /// If there are no reversed connections, return Ok(None).
    ///
    /// This function performs the following steps:
    /// 1. Checks all connections for message flows with source fields
    /// 2. Converts reversed connections (with source) to forward connections
    /// 3. Removes processed source fields
    /// 4. Merges duplicate forward connections if they exist
    ///
    /// # Arguments
    /// * `graph` - The input graph to process
    ///
    /// # Returns
    /// * `Ok(None)` if no reversed connections found
    /// * `Ok(Some(Graph))` with converted graph if reversed connections exist
    /// * `Err` if there are conflicts during merging
    pub fn convert_reversed_connections_to_forward_connections(
        graph: &Graph,
    ) -> Result<Option<Graph>> {
        // Early return if no connections exist
        let Some(connections) = &graph.connections else {
            return Ok(None);
        };

        // Check if any connections have source fields
        let has_reversed = connections.iter().any(|conn| {
            let check_flows = |flows: &Option<Vec<GraphMessageFlow>>| {
                flows.as_ref().is_some_and(|f| {
                    f.iter().any(|flow| !flow.source.is_empty())
                })
            };

            check_flows(&conn.cmd)
                || check_flows(&conn.data)
                || check_flows(&conn.audio_frame)
                || check_flows(&conn.video_frame)
        });

        if !has_reversed {
            return Ok(None);
        }

        // Create a new graph with the same nodes
        let mut new_graph = graph.clone();
        let mut new_connections = Vec::new();

        // Process each connection type (cmd, data, audio_frame, video_frame)
        for conn in connections {
            let mut new_conn = GraphConnection::new(
                conn.loc.app.clone(),
                conn.loc.extension.clone(),
                conn.loc.subgraph.clone(),
            );

            let conn_dest = GraphDestination {
                loc: conn.loc.clone(),
                msg_conversion: None,
            };

            // Process each type of flow
            if let Some(ref cmd_flows) = conn.cmd {
                new_conn.cmd = Self::process_message_flows(
                    cmd_flows,
                    "cmd",
                    &conn_dest,
                    &mut new_connections,
                );
            }

            if let Some(ref data_flows) = conn.data {
                new_conn.data = Self::process_message_flows(
                    data_flows,
                    "data",
                    &conn_dest,
                    &mut new_connections,
                );
            }

            if let Some(ref audio_flows) = conn.audio_frame {
                new_conn.audio_frame = Self::process_message_flows(
                    audio_flows,
                    "audio_frame",
                    &conn_dest,
                    &mut new_connections,
                );
            }

            if let Some(ref video_flows) = conn.video_frame {
                new_conn.video_frame = Self::process_message_flows(
                    video_flows,
                    "video_frame",
                    &conn_dest,
                    &mut new_connections,
                );
            }

            // Only add connection if it still has flows
            if new_conn.cmd.is_some()
                || new_conn.data.is_some()
                || new_conn.audio_frame.is_some()
                || new_conn.video_frame.is_some()
            {
                new_connections.push(new_conn);
            }
        }

        // Merge duplicate forward connections
        let mut merged_connections: HashMap<GraphLoc, GraphConnection> =
            HashMap::new();
        for mut conn in new_connections {
            let key = conn.loc.clone();

            if let Some(existing) = merged_connections.get_mut(&key) {
                // Merge cmd flows
                if let Some(cmd_flows) = conn.cmd.take() {
                    existing.cmd.get_or_insert_with(Vec::new).extend(cmd_flows);
                }
                // Merge data flows
                if let Some(data_flows) = conn.data.take() {
                    existing
                        .data
                        .get_or_insert_with(Vec::new)
                        .extend(data_flows);
                }
                // Merge audio_frame flows
                if let Some(audio_flows) = conn.audio_frame.take() {
                    existing
                        .audio_frame
                        .get_or_insert_with(Vec::new)
                        .extend(audio_flows);
                }
                // Merge video_frame flows
                if let Some(video_flows) = conn.video_frame.take() {
                    existing
                        .video_frame
                        .get_or_insert_with(Vec::new)
                        .extend(video_flows);
                }
            } else {
                merged_connections.insert(key, conn);
            }
        }

        // TODO(xilin): Merge flows with the same source, type and name. If
        // destinations are also the same, determine conflicts based on msg
        // conversion.
        

        // Update the graph with merged connections
        new_graph.connections =
            Some(merged_connections.into_values().collect());

        Ok(Some(new_graph))
    }
}
