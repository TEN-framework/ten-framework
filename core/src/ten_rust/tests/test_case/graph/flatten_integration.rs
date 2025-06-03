//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use ten_rust::graph::{
    connection::{
        GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow,
    },
    node::{GraphNode, GraphNodeType},
    Graph,
};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validate_and_complete_and_flatten_with_subgraphs() {
        // Create a main graph with subgraph nodes
        let mut main_graph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_a".to_string(),
                    addon: Some("addon_a".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Subgraph,
                    name: "subgraph_1".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some("./test_subgraph.json".to_string()),
                },
            ],
            connections: Some(vec![GraphConnection {
                loc: GraphLoc {
                    app: None,
                    extension: Some("ext_a".to_string()),
                    subgraph: None,
                },
                cmd: Some(vec![GraphMessageFlow {
                    name: "test_cmd".to_string(),
                    dest: vec![GraphDestination {
                        loc: GraphLoc {
                            app: None,
                            extension: Some("subgraph_1:ext_b".to_string()),
                            subgraph: None,
                        },
                        msg_conversion: None,
                    }],
                }]),
                data: None,
                audio_frame: None,
                video_frame: None,
            }]),
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test with current_base_dir as None - should fail because subgraph has
        // relative path
        let result = main_graph.validate_and_complete_and_flatten(None);
        assert!(result.is_err());

        // Verify the error message contains information about base_dir being
        // None
        let error_msg = result.err().unwrap().to_string();
        assert!(error_msg
            .contains("base_dir cannot be None when uri is a relative path"));
    }

    #[test]
    fn test_validate_and_complete_and_flatten_without_subgraphs() {
        // Create a graph without subgraph nodes
        let mut graph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_a".to_string(),
                    addon: Some("addon_a".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_b".to_string(),
                    addon: Some("addon_b".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
            ],
            connections: Some(vec![GraphConnection {
                loc: GraphLoc {
                    app: None,
                    extension: Some("ext_a".to_string()),
                    subgraph: None,
                },
                cmd: Some(vec![GraphMessageFlow {
                    name: "test_cmd".to_string(),
                    dest: vec![GraphDestination {
                        loc: GraphLoc {
                            app: None,
                            extension: Some("ext_b".to_string()),
                            subgraph: None,
                        },
                        msg_conversion: None,
                    }],
                }]),
                data: None,
                audio_frame: None,
                video_frame: None,
            }]),
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test with current_base_dir as None - should work fine since no
        // subgraphs
        let result = graph.validate_and_complete_and_flatten(None);
        assert!(result.is_ok());

        // The graph should remain unchanged
        assert_eq!(graph.nodes.len(), 2);
        assert!(graph
            .nodes
            .iter()
            .all(|node| node.type_ == GraphNodeType::Extension));
    }

    #[test]
    fn test_flatten_graph_returns_none_for_no_subgraphs() {
        // Create a graph without subgraph nodes
        let graph = Graph {
            nodes: vec![GraphNode {
                type_: GraphNodeType::Extension,
                name: "ext_a".to_string(),
                addon: Some("addon_a".to_string()),
                extension_group: None,
                app: None,
                property: None,
                source_uri: None,
            }],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Mock subgraph loader (should not be called)
        let subgraph_loader = |_uri: &str,
                               _base_dir: Option<&str>,
                               _new_base_dir: &mut Option<String>|
         -> anyhow::Result<Graph> {
            panic!(
                "Subgraph loader should not be called for graphs without \
                 subgraphs"
            );
        };

        // flatten_graph should return None since there are no subgraphs
        let result = graph.flatten_graph(&subgraph_loader, None).unwrap();
        assert!(result.is_none());
    }

    #[test]
    fn test_flatten_graph_returns_some_for_subgraphs() {
        // Create a graph with subgraph nodes
        let graph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_a".to_string(),
                    addon: Some("addon_a".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Subgraph,
                    name: "subgraph_1".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some("./test_subgraph.json".to_string()),
                },
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Create a simple subgraph
        let subgraph = Graph {
            nodes: vec![GraphNode {
                type_: GraphNodeType::Extension,
                name: "ext_b".to_string(),
                addon: Some("addon_b".to_string()),
                extension_group: None,
                app: None,
                property: None,
                source_uri: None,
            }],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Mock subgraph loader
        let subgraph_loader =
            |_uri: &str,
             _base_dir: Option<&str>,
             _new_base_dir: &mut Option<String>|
             -> anyhow::Result<Graph> { Ok(subgraph.clone()) };

        // flatten_graph should return Some since there are subgraphs
        let result =
            graph.flatten_graph(&subgraph_loader, Some("/tmp")).unwrap();
        assert!(result.is_some());

        let flattened = result.unwrap();
        assert_eq!(flattened.nodes.len(), 2); // ext_a + subgraph_1_ext_b
        assert!(flattened.nodes.iter().any(|node| node.name == "ext_a"));
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_ext_b"));
    }
}
