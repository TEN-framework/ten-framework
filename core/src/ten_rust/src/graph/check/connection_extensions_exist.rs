//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use crate::graph::{connection::GraphMessageFlow, node::GraphNodeType, Graph};

impl Graph {
    fn check_destination_extensions_exist(
        all_extensions: &[String],
        flows: &[GraphMessageFlow],
        conn_idx: usize,
        msg_type: &str,
    ) -> Result<()> {
        for (flow_idx, flow) in flows.iter().enumerate() {
            for (dest_idx, dest) in flow.dest.iter().enumerate() {
                // Get extension name, log error if missing
                let extension_name = match &dest.loc.extension {
                    Some(name) => name,
                    None => {
                        return Err(anyhow::anyhow!(
                            "Missing extension name in connection[{}].{}[{}].dest[{}]",
                            conn_idx,
                            msg_type,
                            flow_idx,
                            dest_idx
                        ));
                    }
                };

                // Skip validation for subgraph namespace references (xxx:yyy
                // format) except for built-in extensions with
                // "ten:" prefix These will be validated by
                // check_subgraph_references_exist
                if let Some(colon_pos) = extension_name.find(':') {
                    let namespace = &extension_name[..colon_pos];
                    if namespace != "ten" {
                        // This is a subgraph namespace reference, skip
                        // extension validation
                        continue;
                    }
                }

                let dest_extension = format!(
                    "{}:{}",
                    dest.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                    extension_name
                );

                if !all_extensions.contains(&dest_extension) {
                    return Err(anyhow::anyhow!(
                        "The extension declared in connections[{}].{}[{}] is not defined in \
                         nodes, extension: {}.",
                        conn_idx,
                        msg_type,
                        flow_idx,
                        extension_name
                    ));
                }
            }
        }

        Ok(())
    }

    /// Checks that all extensions referenced in connections are defined in
    /// nodes.
    ///
    /// This function traverses the graph and ensures that any extension
    /// referenced in a connection has a corresponding node definition. This
    /// check is essential for maintaining graph integrity and preventing
    /// references to non-existent extensions.
    ///
    /// # Returns
    /// - `Ok(())` if all referenced extensions exist
    /// - `Err` with a descriptive error message if any referenced extension is
    ///   not defined
    pub fn check_connection_extensions_exist(&self) -> Result<()> {
        if self.connections.is_none() {
            return Ok(());
        }
        let connections = self.connections.as_ref().unwrap();

        // Build a comprehensive list of all extension identifiers in the graph
        // Each extension is uniquely identified as "app_uri:extension_name"
        let mut all_extensions: Vec<String> = Vec::new();
        for node in &self.nodes {
            if node.get_type() == GraphNodeType::Extension {
                let unique_ext_name = format!(
                    "{}:{}",
                    node.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                    node.get_name()
                );
                all_extensions.push(unique_ext_name);
            }
        }

        // Validate each connection in the graph.
        for (conn_idx, connection) in connections.iter().enumerate() {
            // First, verify the source extension exists.
            if let Some(extension_name) = &connection.loc.extension {
                // Skip validation for subgraph namespace references (xxx:yyy
                // format) except for built-in extensions with
                // "ten:" prefix These will be validated by
                // check_subgraph_references_exist
                let should_skip = if let Some(colon_pos) = extension_name.find(':') {
                    let namespace = &extension_name[..colon_pos];
                    namespace != "ten"
                } else {
                    false
                };

                if !should_skip {
                    let src_extension = format!(
                        "{}:{}",
                        connection.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                        extension_name
                    );
                    if !all_extensions.contains(&src_extension) {
                        return Err(anyhow::anyhow!(
                            "The extension declared in connections[{}] is not defined in nodes, \
                             extension: {}.",
                            conn_idx,
                            extension_name
                        ));
                    }
                }
            }

            // Check all command message flows if present.
            if let Some(cmd_flows) = &connection.cmd {
                Graph::check_destination_extensions_exist(
                    &all_extensions,
                    cmd_flows,
                    conn_idx,
                    "cmd",
                )?;
            }

            // Check all data message flows if present.
            if let Some(data_flows) = &connection.data {
                Graph::check_destination_extensions_exist(
                    &all_extensions,
                    data_flows,
                    conn_idx,
                    "data",
                )?;
            }

            // Check all audio frame message flows if present.
            if let Some(audio_flows) = &connection.audio_frame {
                Graph::check_destination_extensions_exist(
                    &all_extensions,
                    audio_flows,
                    conn_idx,
                    "audio_frame",
                )?;
            }

            // Check all video frame message flows if present.
            if let Some(video_flows) = &connection.video_frame {
                Graph::check_destination_extensions_exist(
                    &all_extensions,
                    video_flows,
                    conn_idx,
                    "video_frame",
                )?;
            }
        }

        Ok(())
    }
}
