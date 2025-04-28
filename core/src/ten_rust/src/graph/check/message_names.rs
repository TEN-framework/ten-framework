//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::{Ok, Result};

use crate::graph::{connection::GraphMessageFlow, Graph, GraphConnection};

impl Graph {
    /// Check that all message names are unique across connections.
    ///
    /// Checks each connection for duplicated message names and collects any
    /// validation errors found.
    ///
    /// # Returns
    /// - `Ok(())` if no duplicates are found
    /// - `Err` with formatted error message if duplicates exist
    fn check_unique_message_names(&self) -> Result<()> {
        let mut errors: Vec<String> = vec![];

        // Iterate through each connection.
        for (conn_idx, connection) in
            self.connections.as_ref().unwrap().iter().enumerate()
        {
            // Check for duplicate message names within this connection.
            if let Some(errs) = self.check_connection_message_names(connection)
            {
                errors.push(format!("- connection[{}]:", conn_idx));
                for err in errs {
                    errors.push(format!("  {}", err));
                }
            }
        }

        if errors.is_empty() {
            Ok(())
        } else {
            Err(anyhow::anyhow!("{}", errors.join("\n")))
        }
    }

    /// Identifies duplicate message names within a specific flow group.
    ///
    /// Maps message names to their first occurrence index and reports any
    /// subsequent occurrences as duplicates.
    ///
    /// # Parameters
    /// - `message_flows`: Message flows to check for duplicates
    ///
    /// # Returns
    /// Errors describing duplicate messages
    fn find_duplicate_names_in_flow_group(
        &self,
        message_flows: &[GraphMessageFlow],
    ) -> Vec<String> {
        let mut errors: Vec<String> = vec![];

        // HashMap to track first occurrence of each message name.
        let mut msg_names: HashMap<String, usize> = HashMap::new();

        // Iterate through each message flow in the group.
        for (flow_idx, flow) in message_flows.iter().enumerate() {
            // Check if the message name has already been seen.
            if let Some(idx) = msg_names.get(&flow.name) {
                errors.push(format!(
                    "'{}' is defined in flow[{}] and flow[{}].",
                    flow.name, idx, flow_idx
                ));
            } else {
                // Record the first occurrence of the message name.
                msg_names.insert(flow.name.clone(), flow_idx);
            }
        }

        errors
    }

    /// Check message name uniqueness within a single connection.
    ///
    /// Checks each message type (cmd, data, audio_frame, video_frame)
    /// individually for duplicate names.
    ///
    /// # Parameters
    /// - `connection`: Connection to validate for duplicate message names
    ///
    /// # Returns
    /// - `None` if no duplicates are found
    /// - `Some(Vec<String>)` with error messages if duplicates exist
    fn check_connection_message_names(
        &self,
        connection: &GraphConnection,
    ) -> Option<Vec<String>> {
        let mut errors: Vec<String> = vec![];

        // Check command message flows for duplicates.
        if let Some(cmd) = &connection.cmd {
            let errs = self.find_duplicate_names_in_flow_group(cmd);
            if !errs.is_empty() {
                errors.push(
                    "- Merge the following cmd into one section:".to_string(),
                );
                for err in errs {
                    errors.push(format!("  {}", err));
                }
            }
        }

        // Check data message flows for duplicates.
        if let Some(data) = &connection.data {
            let errs = self.find_duplicate_names_in_flow_group(data);
            if !errs.is_empty() {
                errors.push(
                    "- Merge the following data into one section:".to_string(),
                );
                for err in errs {
                    errors.push(format!("  {}", err));
                }
            }
        }

        // Check audio frame message flows for duplicates.
        if let Some(audio_frame) = &connection.audio_frame {
            let errs = self.find_duplicate_names_in_flow_group(audio_frame);
            if !errs.is_empty() {
                errors.push(
                    "- Merge the following auto_frame into one section:"
                        .to_string(),
                );
                for err in errs {
                    errors.push(format!("  {}", err));
                }
            }
        }

        // Check video frame message flows for duplicates.
        if let Some(video_frame) = &connection.video_frame {
            let errs = self.find_duplicate_names_in_flow_group(video_frame);
            if !errs.is_empty() {
                errors.push(
                    "- Merge the following video_frame into one section:"
                        .to_string(),
                );
                for err in errs {
                    errors.push(format!("  {}", err));
                }
            }
        }

        if errors.is_empty() {
            None
        } else {
            Some(errors)
        }
    }

    /// Check message names in the graph.
    ///
    /// # Returns
    /// - `Ok(())` if validation passes
    /// - `Err` with comprehensive error message if check fails
    pub fn check_message_names(&self) -> Result<()> {
        if self.connections.is_none() {
            return Ok(());
        }

        // Check unique message names across all connections.
        self.check_unique_message_names()?;

        Ok(())
    }
}
