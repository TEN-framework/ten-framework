//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use ten_rust::{
    graph::{node::GraphNode, Graph},
    pkg_info::{pkg_type::PkgType, pkg_type_and_name::PkgTypeAndName},
};

/// Checks if a node exists in the graph.
fn check_node_exist(
    graph: &Graph,
    app: &Option<String>,
    extension: &str,
) -> Result<()> {
    // Validate that source node exists.
    let src_node_exists = graph
        .nodes
        .iter()
        .any(|node| node.type_and_name.name == extension && node.app == *app);

    if src_node_exists {
        return Err(anyhow::anyhow!(
            "Node with extension '{}' and app '{:?}' already exists in the \
             graph",
            extension,
            app
        ));
    }

    Ok(())
}

pub fn graph_add_extension_node(
    graph: &mut Graph,
    pkg_name: &str,
    addon: &str,
    app: &Option<String>,
    extension_group: &Option<String>,
    property: &Option<serde_json::Value>,
) -> Result<()> {
    check_node_exist(graph, app, pkg_name)?;

    // Store the original state in case validation fails.
    let original_graph = graph.clone();

    // Create new GraphNode.
    let node = GraphNode {
        type_and_name: PkgTypeAndName {
            pkg_type: PkgType::Extension,
            name: pkg_name.to_string(),
        },
        addon: addon.to_string(),
        extension_group: extension_group.clone(),
        app: app.clone(),
        property: property.clone(),
    };

    // Add the node to the graph.
    graph.nodes.push(node);

    // Validate the graph.
    match graph.validate_and_complete() {
        Ok(_) => Ok(()),
        Err(e) => {
            // Restore the original graph if validation fails.
            *graph = original_graph;
            Err(e)
        }
    }
}
