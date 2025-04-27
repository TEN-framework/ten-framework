//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::{Ok, Result};

use crate::graph::Graph;

impl Graph {
    /// Check that extension identifiers are unique across all connections in
    /// the graph.
    ///
    /// In TEN Framework, each extension should appear only once in the
    /// connections list to maintain a clear and unambiguous command routing
    /// structure. When multiple commands need to be associated with the same
    /// extension, they should be grouped under a single connection entry.
    ///
    /// # Returns
    /// - `Ok(())` if all extensions appear exactly once
    /// - `Err(anyhow::Error)` with detailed error messages if duplicates are
    ///   found
    ///
    /// # Example
    /// The following connection is invalid.
    ///
    /// "connections": [{
    ///   "extension": "some_ext",
    ///   "cmd": [{
    ///     "name": "some_cmd",
    ///     "dest": [{
    ///       "extension": "ext_2"
    ///     }]
    ///   }]
    /// }, {
    ///   "extension": "some_ext",   // <== duplicate extension
    ///   "cmd": [{
    ///     "name": "another_cmd",
    ///     "dest": [{
    ///       "extension": "ext_3"
    ///     }]
    ///   }]
    /// }]
    pub fn check_extension_uniqueness_in_connections(&self) -> Result<()> {
        let mut errors: Vec<String> = vec![];

        // HashMap to track first occurrence of each extension.
        let mut extensions: HashMap<String, usize> = HashMap::new();

        for (conn_idx, connection) in
            self.connections.as_ref().unwrap().iter().enumerate()
        {
            // Create a unique identifier for the extension including app URI.
            let extension = format!(
                "{}:{}",
                connection.get_app_uri().as_ref().map_or("", |s| s.as_str()),
                connection.extension
            );

            // Check if we've seen this extension before.
            if let Some(idx) = extensions.get(&extension) {
                errors.push(format!(
                    "extension '{}' is defined in connection[{}] and \
                     connection[{}], merge them into one section.",
                    connection.extension, idx, conn_idx
                ));
            } else {
                // Record the first occurrence of the extension.
                extensions.insert(extension, conn_idx);
            }
        }

        if errors.is_empty() {
            Ok(())
        } else {
            Err(anyhow::anyhow!("{}", errors.join("\n")))
        }
    }
}
