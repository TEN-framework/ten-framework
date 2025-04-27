//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use crate::{graph::Graph, pkg_info::pkg_type::PkgType};

impl Graph {
    /// Validates that at least one extension node exists in the graph.
    ///
    /// # Returns
    /// - `Ok(())` if at least one extension exists
    /// - `Err` with descriptive message if no extensions are found
    pub fn check_extension_existence(&self) -> Result<()> {
        // Check if any node in the graph is an extension.
        let extension_exists = self
            .nodes
            .iter()
            .any(|node| node.type_and_name.pkg_type == PkgType::Extension);

        if !extension_exists {
            return Err(anyhow::anyhow!(
                "No extension node is defined in graph."
            ));
        }

        Ok(())
    }
}
