//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use crate::{
    graph::{connection::GraphMessageFlow, Graph},
    pkg_info::pkg_type::PkgType,
};

impl Graph {
    fn check_destination_extensions_exist(
        all_extensions: &[String],
        flows: &[GraphMessageFlow],
        conn_idx: usize,
        msg_type: &str,
    ) -> Result<()> {
        for (flow_idx, flow) in flows.iter().enumerate() {
            for dest in &flow.dest {
                let dest_extension = format!(
                    "{}:{}",
                    dest.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                    dest.extension
                );

                if !all_extensions.contains(&dest_extension) {
                    return Err(anyhow::anyhow!(
                        "The extension declared in connections[{}].{}[{}] is \
                         not defined in nodes, extension: {}.",
                        conn_idx,
                        msg_type,
                        flow_idx,
                        dest.extension
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
            if node.type_and_name.pkg_type == PkgType::Extension {
                let unique_ext_name = format!(
                    "{}:{}",
                    node.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                    node.type_and_name.name
                );
                all_extensions.push(unique_ext_name);
            }
        }

        // Validate each connection in the graph.
        for (conn_idx, connection) in connections.iter().enumerate() {
            // First, verify the source extension exists.
            let src_extension = format!(
                "{}:{}",
                connection.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                connection.extension
            );
            if !all_extensions.contains(&src_extension) {
                return Err(anyhow::anyhow!(
                    "The extension declared in connections[{}] is not defined \
                     in nodes, extension: {}.",
                    conn_idx,
                    connection.extension
                ));
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
