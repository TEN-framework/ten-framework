//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::Result;
use uuid::Uuid;

use crate::graph::{
    connection::GraphConnection, graph_info::GraphInfo, node::GraphNode, Graph,
};

pub fn graphs_cache_find<F>(
    graphs_cache: &HashMap<Uuid, GraphInfo>,
    predicate: F,
) -> Option<&GraphInfo>
where
    F: Fn(&GraphInfo) -> bool,
{
    graphs_cache.iter().find_map(|(_, graph)| {
        if predicate(graph) {
            Some(graph)
        } else {
            None
        }
    })
}

pub fn graphs_cache_find_mut<F>(
    graphs_cache: &mut HashMap<Uuid, GraphInfo>,
    predicate: F,
) -> Option<&mut GraphInfo>
where
    F: Fn(&mut GraphInfo) -> bool,
{
    graphs_cache.iter_mut().find_map(|(_, graph)| {
        if predicate(graph) {
            Some(graph)
        } else {
            None
        }
    })
}

pub fn get_pkg_predefined_graph_from_nodes_and_connections(
    graph_name: &str,
    auto_start: bool,
    nodes: &[GraphNode],
    connections: &[GraphConnection],
) -> Result<GraphInfo> {
    Ok(GraphInfo {
        name: Some(graph_name.to_string()),
        auto_start: Some(auto_start),
        graph: Graph {
            nodes: nodes.to_vec(),
            connections: Some(connections.to_vec()),
        },
        app_base_dir: None,
        belonging_pkg_type: None,
        belonging_pkg_name: None,
    })
}
