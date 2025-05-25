//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use anyhow::Result;

    use ten_rust::graph::{
        connection::{self, GraphConnection},
        node::{GraphNode, GraphNodeType},
        Graph,
    };

    #[test]
    fn test_flatten_basic_subgraph() {
        // Create a main graph with a subgraph node
        let main_graph = Graph {
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
                    property: Some(
                        serde_json::json!({"app_id": "${env:AGORA_APP_ID}"}),
                    ),
                    source_uri: Some(
                        "http://example.com/subgraph.json".to_string(),
                    ),
                },
            ],
            connections: Some(vec![GraphConnection {
                loc: connection::GraphLoc {
                    app: None,
                    extension: Some("ext_a".to_string()),
                    subgraph: None,
                },
                cmd: Some(vec![connection::GraphMessageFlow {
                    name: "B".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: Some("subgraph_1:ext_d".to_string()),
                            subgraph: None,
                        },
                        msg_conversion: None,
                    }],
                }]),
                data: None,
                audio_frame: None,
                video_frame: None,
            }]),
            exposed_messages: Some(vec![]),
            exposed_properties: Some(vec![]),
        };

        // Create a subgraph to be loaded
        let subgraph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_c".to_string(),
                    addon: Some("addon_c".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_d".to_string(),
                    addon: Some("addon_d".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
            ],
            connections: Some(vec![GraphConnection {
                loc: connection::GraphLoc {
                    app: None,
                    extension: Some("ext_c".to_string()),
                    subgraph: None,
                },
                cmd: Some(vec![connection::GraphMessageFlow {
                    name: "B".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: Some("ext_d".to_string()),
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

        // Mock subgraph loader
        let subgraph_loader =
            |_uri: &str| -> Result<Graph> { Ok(subgraph.clone()) };

        // Flatten the graph
        let flattened = main_graph.flatten(subgraph_loader).unwrap();

        // Verify results
        assert_eq!(flattened.nodes.len(), 3); // ext_a + 2 from subgraph

        // Check that original extension is preserved
        assert!(flattened.nodes.iter().any(|node| node.name == "ext_a"
            && node.addon == Some("addon_a".to_string())));

        // Check that subgraph extensions are flattened with prefix
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_ext_c"
                && node.addon == Some("addon_c".to_string())));
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_ext_d"
                && node.addon == Some("addon_d".to_string())));

        // Check that properties are merged correctly
        let ext_d_node = flattened
            .nodes
            .iter()
            .find(|node| node.name == "subgraph_1_ext_d")
            .unwrap();
        assert!(ext_d_node.property.is_some());
        assert_eq!(
            ext_d_node.property.as_ref().unwrap()["app_id"],
            "${env:AGORA_APP_ID}"
        );

        // Check that connections are flattened
        let connections = flattened.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 2); // Original + internal subgraph connection

        // Check that colon notation is converted to underscore
        let main_connection = connections
            .iter()
            .find(|conn| conn.loc.extension.as_deref() == Some("ext_a"))
            .unwrap();
        let cmd_flow = &main_connection.cmd.as_ref().unwrap()[0];
        assert_eq!(
            cmd_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_1_ext_d"
        );

        // Check internal subgraph connection is preserved
        let internal_connection = connections
            .iter()
            .find(|conn| {
                conn.loc.extension.as_deref() == Some("subgraph_1_ext_c")
            })
            .unwrap();
        assert!(internal_connection.cmd.is_some());

        // Check that exposed_messages and exposed_properties are discarded
        assert!(flattened.exposed_messages.is_none());
        assert!(flattened.exposed_properties.is_none());
    }

    #[test]
    fn test_flatten_nested_subgraphs_error() {
        let main_graph = Graph {
            nodes: vec![GraphNode {
                type_: GraphNodeType::Subgraph,
                name: "subgraph_1".to_string(),
                addon: None,
                extension_group: None,
                app: None,
                property: None,
                source_uri: Some(
                    "http://example.com/subgraph.json".to_string(),
                ),
            }],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Create a subgraph that contains another subgraph (nested)
        let nested_subgraph = Graph {
            nodes: vec![GraphNode {
                type_: GraphNodeType::Subgraph,
                name: "nested_subgraph".to_string(),
                addon: None,
                extension_group: None,
                app: None,
                property: None,
                source_uri: Some("http://example.com/nested.json".to_string()),
            }],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let subgraph_loader =
            |_uri: &str| -> Result<Graph> { Ok(nested_subgraph.clone()) };

        // This should return an error because nested subgraphs are not
        // supported
        let result = main_graph.flatten(subgraph_loader);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Nested subgraphs are not supported"));
    }

    #[test]
    fn test_flatten_missing_source_uri_error() {
        let main_graph = Graph {
            nodes: vec![GraphNode {
                type_: GraphNodeType::Subgraph,
                name: "subgraph_1".to_string(),
                addon: None,
                extension_group: None,
                app: None,
                property: None,
                source_uri: None, // Missing source_uri
            }],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let subgraph_loader = |_uri: &str| -> Result<Graph> {
            unreachable!("Should not be called")
        };

        // This should return an error because source_uri is missing
        let result = main_graph.flatten(subgraph_loader);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("must have source_uri"));
    }
}
