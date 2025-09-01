//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use crate::graph::{connection::GraphMessageFlow, node::GraphNodeType, Graph};

impl Graph {
    /// Creates a subgraph identifier in the format "app_uri:subgraph_name"
    fn create_subgraph_identifier(app_uri: Option<&String>, subgraph_name: &str) -> String {
        format!("{}:{}", app_uri.map_or("", |s| s.as_str()), subgraph_name)
    }

    /// Validates that a direct subgraph reference exists in the list of all
    /// subgraphs
    fn validate_direct_subgraph_reference(
        all_subgraphs: &[String],
        app_uri: Option<&String>,
        subgraph_name: &str,
        error_context: &str,
    ) -> Result<()> {
        let subgraph_identifier = Self::create_subgraph_identifier(app_uri, subgraph_name);

        if !all_subgraphs.contains(&subgraph_identifier) {
            return Err(anyhow::anyhow!(
                "The subgraph '{subgraph_name}' {error_context} is not defined in nodes.",
            ));
        }

        Ok(())
    }

    /// Validates that a subgraph referenced through extension namespace exists
    fn validate_extension_namespace_subgraph_reference(
        all_subgraphs: &[String],
        app_uri: Option<&String>,
        extension_name: &str,
        error_context: &str,
    ) -> Result<()> {
        if let Some(colon_pos) = extension_name.find(':') {
            let subgraph_name = &extension_name[..colon_pos];

            // Skip validation for built-in extensions with "ten:" prefix
            if subgraph_name != "ten" {
                let subgraph_identifier = Self::create_subgraph_identifier(app_uri, subgraph_name);

                if !all_subgraphs.contains(&subgraph_identifier) {
                    return Err(anyhow::anyhow!(
                        "The subgraph '{subgraph_name}' {error_context} (from extension \
                         '{extension_name}') is not defined in nodes.",
                    ));
                }
            }
        }

        Ok(())
    }

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
                    let error_context = format!(
                        "referenced in \
                         connections[{conn_idx}].{msg_type}[{flow_idx}].dest[{dest_idx}]",
                    );

                    Self::validate_direct_subgraph_reference(
                        all_subgraphs,
                        dest.get_app_uri().as_ref(),
                        subgraph_name,
                        &error_context,
                    )?;
                }

                // Check if extension name contains subgraph namespace (xxx:yyy
                // format)
                if let Some(extension_name) = &dest.loc.extension {
                    let error_context = format!(
                        "referenced in \
                         connections[{conn_idx}].{msg_type}[{flow_idx}].dest[{dest_idx}]",
                    );

                    Self::validate_extension_namespace_subgraph_reference(
                        all_subgraphs,
                        dest.get_app_uri().as_ref(),
                        extension_name,
                        &error_context,
                    )?;
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
            if node.get_type() == GraphNodeType::Subgraph {
                let unique_subgraph_name =
                    Self::create_subgraph_identifier(node.get_app_uri().as_ref(), node.get_name());
                all_subgraphs.push(unique_subgraph_name);
            }
        }

        // Validate each connection in the graph.
        for (conn_idx, connection) in connections.iter().enumerate() {
            // Check if the source connection references a subgraph directly
            if let Some(subgraph_name) = &connection.loc.subgraph {
                let error_context = format!("declared in connections[{conn_idx}]");

                Self::validate_direct_subgraph_reference(
                    &all_subgraphs,
                    connection.get_app_uri().as_ref(),
                    subgraph_name,
                    &error_context,
                )?;
            }

            // Check if the source extension contains subgraph namespace
            if let Some(extension_name) = &connection.loc.extension {
                let error_context = format!("referenced in connections[{conn_idx}]");

                Self::validate_extension_namespace_subgraph_reference(
                    &all_subgraphs,
                    connection.get_app_uri().as_ref(),
                    extension_name,
                    &error_context,
                )?;
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
