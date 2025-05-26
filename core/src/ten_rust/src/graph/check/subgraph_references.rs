//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use crate::graph::{connection::GraphMessageFlow, node::GraphNodeType, Graph};

impl Graph {
    fn check_destination_subgraph_references_exist(
        all_subgraphs: &[String],
        flows: &[GraphMessageFlow],
        conn_idx: usize,
        msg_type: &str,
    ) -> Result<()> {
        for (flow_idx, flow) in flows.iter().enumerate() {
            for (dest_idx, dest) in flow.dest.iter().enumerate() {
                // Check if destination references a subgraph directly
                if let Some(subgraph_name) = &dest.loc.subgraph {
                    let subgraph_identifier = format!(
                        "{}:{}",
                        dest.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                        subgraph_name
                    );

                    if !all_subgraphs.contains(&subgraph_identifier) {
                        return Err(anyhow::anyhow!(
                            "The subgraph '{}' referenced in \
                             connections[{}].{}[{}].dest[{}] is not defined \
                             in nodes.",
                            subgraph_name,
                            conn_idx,
                            msg_type,
                            flow_idx,
                            dest_idx
                        ));
                    }
                }

                // Check if extension name contains subgraph namespace (xxx:yyy
                // format) Exclude built-in extensions that start with "ten:"
                if let Some(extension_name) = &dest.loc.extension {
                    if let Some(colon_pos) = extension_name.find(':') {
                        let subgraph_name = &extension_name[..colon_pos];

                        // Skip validation for built-in extensions with "ten:"
                        // prefix
                        if subgraph_name != "ten" {
                            let subgraph_identifier = format!(
                                "{}:{}",
                                dest.get_app_uri()
                                    .as_ref()
                                    .map_or("", |s| s.as_str()),
                                subgraph_name
                            );

                            if !all_subgraphs.contains(&subgraph_identifier) {
                                return Err(anyhow::anyhow!(
                                    "The subgraph '{}' referenced in \
                                     connections[{}].{}[{}].dest[{}] (from \
                                     extension '{}') is not defined in nodes.",
                                    subgraph_name,
                                    conn_idx,
                                    msg_type,
                                    flow_idx,
                                    dest_idx,
                                    extension_name
                                ));
                            }
                        }
                    }
                }
            }
        }

        Ok(())
    }

    /// Checks that all subgraphs referenced in connections are defined in
    /// nodes.
    ///
    /// This function validates two types of subgraph references:
    /// 1. Direct subgraph references using the "subgraph" field
    /// 2. Namespace references in extension names using "xxx:yyy" format where
    ///    "xxx" is the subgraph name
    ///
    /// When connections reference subgraphs either directly or through
    /// namespace syntax, the corresponding subgraph nodes must be defined
    /// in the nodes array with type "subgraph".
    pub fn check_subgraph_references_exist(&self) -> Result<()> {
        if self.connections.is_none() {
            return Ok(());
        }
        let connections = self.connections.as_ref().unwrap();

        // Build a comprehensive list of all subgraph identifiers in the graph
        // Each subgraph is uniquely identified as "app_uri:subgraph_name"
        let mut all_subgraphs: Vec<String> = Vec::new();
        for node in &self.nodes {
            if node.type_ == GraphNodeType::Subgraph {
                let unique_subgraph_name = format!(
                    "{}:{}",
                    node.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                    node.name
                );
                all_subgraphs.push(unique_subgraph_name);
            }
        }

        // Validate each connection in the graph.
        for (conn_idx, connection) in connections.iter().enumerate() {
            // Check if the source connection references a subgraph directly
            if let Some(subgraph_name) = &connection.loc.subgraph {
                let src_subgraph = format!(
                    "{}:{}",
                    connection
                        .get_app_uri()
                        .as_ref()
                        .map_or("", |s| s.as_str()),
                    subgraph_name
                );
                if !all_subgraphs.contains(&src_subgraph) {
                    return Err(anyhow::anyhow!(
                        "The subgraph '{}' declared in connections[{}] is not \
                         defined in nodes.",
                        subgraph_name,
                        conn_idx
                    ));
                }
            }

            // Check if the source extension contains subgraph namespace
            if let Some(extension_name) = &connection.loc.extension {
                if let Some(colon_pos) = extension_name.find(':') {
                    let subgraph_name = &extension_name[..colon_pos];

                    // Skip validation for built-in extensions with "ten:"
                    // prefix
                    if subgraph_name != "ten" {
                        let src_subgraph = format!(
                            "{}:{}",
                            connection
                                .get_app_uri()
                                .as_ref()
                                .map_or("", |s| s.as_str()),
                            subgraph_name
                        );
                        if !all_subgraphs.contains(&src_subgraph) {
                            return Err(anyhow::anyhow!(
                                "The subgraph '{}' referenced in \
                                 connections[{}] (from extension '{}') is not \
                                 defined in nodes.",
                                subgraph_name,
                                conn_idx,
                                extension_name
                            ));
                        }
                    }
                }
            }

            // Check all command message flows if present.
            if let Some(cmd_flows) = &connection.cmd {
                Graph::check_destination_subgraph_references_exist(
                    &all_subgraphs,
                    cmd_flows,
                    conn_idx,
                    "cmd",
                )?;
            }

            // Check all data message flows if present.
            if let Some(data_flows) = &connection.data {
                Graph::check_destination_subgraph_references_exist(
                    &all_subgraphs,
                    data_flows,
                    conn_idx,
                    "data",
                )?;
            }

            // Check all audio frame message flows if present.
            if let Some(audio_flows) = &connection.audio_frame {
                Graph::check_destination_subgraph_references_exist(
                    &all_subgraphs,
                    audio_flows,
                    conn_idx,
                    "audio_frame",
                )?;
            }

            // Check all video frame message flows if present.
            if let Some(video_flows) = &connection.video_frame {
                Graph::check_destination_subgraph_references_exist(
                    &all_subgraphs,
                    video_flows,
                    conn_idx,
                    "video_frame",
                )?;
            }
        }

        Ok(())
    }
}
