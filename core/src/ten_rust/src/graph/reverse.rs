//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::Result;

use crate::graph::{
    connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow},
    Graph,
};

impl Graph {
    /// Helper function to process a single type of message flows
    /// If flows contain reverse connections, create a new connection from
    /// source to conn_loc and add it to new_connections
    fn process_message_flows(
        flows: &[GraphMessageFlow],
        flow_type: &str,
        conn_loc: &GraphDestination,
        new_connections: &mut Vec<GraphConnection>,
    ) -> Result<()> {
        for flow in flows {
            if flow.source.is_empty() {
                continue;
            }

            // Create forward connections for each source
            for src in &flow.source {
                let mut forward_conn = GraphConnection::new(src.loc.clone());

                // Create a new message flow with the current destinations
                let mut msg_flows = Vec::new();
                let forward_flow = GraphMessageFlow {
                    name: flow.name.clone(),
                    names: None,
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

        Ok(())
    }

    /// Checks if any connections in the graph have source fields
    fn has_reversed_connections(connections: &[GraphConnection]) -> bool {
        connections.iter().any(|conn| {
            let check_flows = |flows: &Option<Vec<GraphMessageFlow>>| {
                flows.as_ref().is_some_and(|f| f.iter().any(|flow| !flow.source.is_empty()))
            };

            check_flows(&conn.cmd)
                || check_flows(&conn.data)
                || check_flows(&conn.audio_frame)
                || check_flows(&conn.video_frame)
        })
    }

    /// If there are reverse connections in a connection, directly flip it to
    /// generate new connections and add them to result connections
    fn reverse_connection(conn: &GraphConnection) -> Result<Vec<GraphConnection>> {
        let mut new_conns = Vec::new();
        let conn_loc = GraphDestination {
            loc: GraphLoc {
                app: conn.loc.app.clone(),
                extension: conn.loc.extension.clone(),
                subgraph: conn.loc.subgraph.clone(),
                selector: conn.loc.selector.clone(),
            },
            msg_conversion: None,
        };

        // Process command flows
        if let Some(cmd_flows) = &conn.cmd {
            Self::process_message_flows(cmd_flows, "cmd", &conn_loc, &mut new_conns)?;
        }

        // Process data flows
        if let Some(data_flows) = &conn.data {
            Self::process_message_flows(data_flows, "data", &conn_loc, &mut new_conns)?;
        }

        // Process audio frame flows
        if let Some(audio_flows) = &conn.audio_frame {
            Self::process_message_flows(audio_flows, "audio_frame", &conn_loc, &mut new_conns)?;
        }

        // Process video frame flows
        if let Some(video_flows) = &conn.video_frame {
            Self::process_message_flows(video_flows, "video_frame", &conn_loc, &mut new_conns)?;
        }

        Ok(new_conns)
    }

    /// Merge flows within a connection to handle duplicates
    fn merge_flows(flows: &mut Vec<GraphMessageFlow>) -> Result<()> {
        let mut flow_map: HashMap<(Vec<GraphLoc>, Option<String>), GraphMessageFlow> =
            HashMap::new();

        for flow in flows.drain(..) {
            let sources: Vec<GraphLoc> = flow.source.iter().map(|src| src.loc.clone()).collect();
            let key = (sources, flow.name.clone());

            if let Some(existing) = flow_map.get_mut(&key) {
                // Merge destinations if they don't exist
                for dest in flow.dest {
                    if !existing.dest.iter().any(|d| d.loc == dest.loc) {
                        // If destination exists with different msg_conversion,
                        // it's a conflict
                        if let Some(existing_dest) = existing
                            .dest
                            .iter()
                            .find(|d| d.loc == dest.loc && d.msg_conversion != dest.msg_conversion)
                        {
                            return Err(anyhow::anyhow!(
                                "Conflicting message conversion for destination {:?}: {:?} vs {:?}",
                                dest.loc,
                                existing_dest.msg_conversion,
                                dest.msg_conversion
                            ));
                        }
                        existing.dest.push(dest);
                    }
                }
            } else {
                flow_map.insert(key, flow);
            }
        }

        *flows = flow_map.into_values().collect();
        Ok(())
    }

    /// Merge duplicate forward connections and their flows
    fn merge_connections(connections: Vec<GraphConnection>) -> Result<Vec<GraphConnection>> {
        let mut merged_connections: HashMap<GraphLoc, GraphConnection> = HashMap::new();

        for mut conn in connections {
            let key = conn.loc.clone();

            if let Some(existing) = merged_connections.get_mut(&key) {
                // Merge cmd flows
                if let Some(cmd_flows) = conn.cmd.take() {
                    existing.cmd.get_or_insert_with(Vec::new).extend(cmd_flows);
                }
                // Merge data flows
                if let Some(data_flows) = conn.data.take() {
                    existing.data.get_or_insert_with(Vec::new).extend(data_flows);
                }
                // Merge audio_frame flows
                if let Some(audio_flows) = conn.audio_frame.take() {
                    existing.audio_frame.get_or_insert_with(Vec::new).extend(audio_flows);
                }
                // Merge video_frame flows
                if let Some(video_flows) = conn.video_frame.take() {
                    existing.video_frame.get_or_insert_with(Vec::new).extend(video_flows);
                }
            } else {
                merged_connections.insert(key, conn);
            }
        }

        // Merge flows within each connection
        for conn in merged_connections.values_mut() {
            if let Some(ref mut cmd_flows) = conn.cmd {
                Self::merge_flows(cmd_flows)?;
            }
            if let Some(ref mut data_flows) = conn.data {
                Self::merge_flows(data_flows)?;
            }
            if let Some(ref mut audio_flows) = conn.audio_frame {
                Self::merge_flows(audio_flows)?;
            }
            if let Some(ref mut video_flows) = conn.video_frame {
                Self::merge_flows(video_flows)?;
            }
        }

        Ok(merged_connections.into_values().collect())
    }

    /// Clears all source arrays in message flows of a connection. If the flow
    /// has no source and dest after clearing, the flow should be removed.
    fn clear_connection_sources(conn: &mut GraphConnection) {
        // Clear sources in command flows
        if let Some(flows) = &mut conn.cmd {
            for flow in flows {
                flow.source.clear();
            }
        }

        // Clear sources in data flows
        if let Some(flows) = &mut conn.data {
            for flow in flows {
                flow.source.clear();
            }
        }

        // Clear sources in audio frame flows
        if let Some(flows) = &mut conn.audio_frame {
            for flow in flows {
                flow.source.clear();
            }
        }

        // Clear sources in video frame flows
        if let Some(flows) = &mut conn.video_frame {
            for flow in flows {
                flow.source.clear();
            }
        }
    }

    /// Removes flows that have no source and no dest from a connection.
    /// If all flows of a certain type are removed, sets that type to None.
    fn remove_empty_flows(conn: &mut GraphConnection) {
        // Helper closure to remove empty flows from a Vec<GraphMessageFlow>
        let remove_empty = |flows: &mut Vec<GraphMessageFlow>| {
            flows.retain(|flow| !flow.source.is_empty() || !flow.dest.is_empty());
        };

        // Remove empty command flows
        if let Some(flows) = &mut conn.cmd {
            remove_empty(flows);
            if flows.is_empty() {
                conn.cmd = None;
            }
        }

        // Remove empty data flows
        if let Some(flows) = &mut conn.data {
            remove_empty(flows);
            if flows.is_empty() {
                conn.data = None;
            }
        }

        // Remove empty audio frame flows
        if let Some(flows) = &mut conn.audio_frame {
            remove_empty(flows);
            if flows.is_empty() {
                conn.audio_frame = None;
            }
        }

        // Remove empty video frame flows
        if let Some(flows) = &mut conn.video_frame {
            remove_empty(flows);
            if flows.is_empty() {
                conn.video_frame = None;
            }
        }
    }

    /// Removes flows that have no source and no dest from a connection.
    /// If all flows of a certain type are removed, sets that type to None.
    fn remove_empty_flows_and_connections(connections: &mut Vec<GraphConnection>) {
        for conn in connections.iter_mut() {
            Self::remove_empty_flows(conn);
        }

        connections.retain(|conn| {
            conn.cmd.is_some()
                || conn.data.is_some()
                || conn.audio_frame.is_some()
                || conn.video_frame.is_some()
        });
    }

    /// Convert reversed connections to forward connections.
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
    pub fn convert_reversed_connections_to_forward_connections(&self) -> Result<Option<Graph>> {
        // Early return if no connections exist
        let Some(connections) = &self.connections else {
            return Ok(None);
        };

        // Check if any connections have source fields
        if !Self::has_reversed_connections(connections) {
            return Ok(None);
        }

        // Create a new graph with the same nodes
        let mut new_graph = self.clone();

        // Add original connections to new_connections. We don't care about
        // the reverse flows specified by source, so we have to remove all
        // source fields from the connections.
        let mut new_connections = connections.to_vec();

        // Clear all source arrays in message flows
        for conn in &mut new_connections {
            Self::clear_connection_sources(conn);
        }

        // Reverse connections and add reversed connections to new_connections
        for conn in connections {
            let reversed_conns = Self::reverse_connection(conn)?;
            new_connections.extend(reversed_conns);
        }

        // Merge duplicate forward connections and their flows
        let mut merged_connections = Self::merge_connections(new_connections)?;

        // Remove all flows with no source and dest, and if all flows of a
        // certain type are removed, sets that type to None. If all flows of
        // all types are removed, the connection should be removed.
        Self::remove_empty_flows_and_connections(&mut merged_connections);

        new_graph.connections = Some(merged_connections);

        Ok(Some(new_graph))
    }
}
