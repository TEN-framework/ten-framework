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
        graph_info::load_graph_from_uri,
        node::{GraphNode, GraphNodeType},
        Graph, GraphExposedMessage, GraphExposedMessageType,
        GraphExposedProperty,
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
                            extension: Some("subgraph_1_ext_d".to_string()),
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
            exposed_properties: Some(vec![GraphExposedProperty {
                extension: Some("ext_d".to_string()),
                name: "app_id".to_string(),
                subgraph: None,
            }]),
        };

        // Mock subgraph loader
        let subgraph_loader =
            |_uri: &str,
             _base_dir: Option<&str>,
             _new_base_dir: &mut Option<String>|
             -> Result<Graph> { Ok(subgraph.clone()) };

        // Flatten the graph
        let flattened =
            main_graph.flatten_graph(&subgraph_loader, None).unwrap().unwrap();

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

        // Check that the connection destination is correct
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
    fn test_flatten_subgraph_field_reference() {
        // Create a main graph with subgraph field references
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
                    name: "subgraph_2".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph2.json".to_string(),
                    ),
                },
            ],
            connections: Some(vec![
                // ext_a sends cmd B to subgraph_2 (should resolve to ext_d via
                // exposed_messages)
                GraphConnection {
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
                                extension: None,
                                subgraph: Some("subgraph_2".to_string()),
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    data: None,
                    audio_frame: None,
                    video_frame: None,
                },
                // subgraph_2 sends cmd H to ext_a (should resolve to ext_c via
                // exposed_messages)
                GraphConnection {
                    loc: connection::GraphLoc {
                        app: None,
                        extension: None,
                        subgraph: Some("subgraph_2".to_string()),
                    },
                    cmd: Some(vec![connection::GraphMessageFlow {
                        name: "H".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: Some("ext_a".to_string()),
                                subgraph: None,
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    data: None,
                    audio_frame: None,
                    video_frame: None,
                },
            ]),
            exposed_messages: None,
            exposed_properties: Some(vec![GraphExposedProperty {
                extension: Some("ext_d".to_string()),
                name: "app_id".to_string(),
                subgraph: None,
            }]),
        };

        // Create a subgraph with exposed_messages
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
            connections: None,
            exposed_messages: Some(vec![
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdIn,
                    name: "B".to_string(),
                    extension: Some("ext_d".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdOut,
                    name: "H".to_string(),
                    extension: Some("ext_c".to_string()),
                    subgraph: None,
                },
            ]),
            exposed_properties: None,
        };

        // Mock subgraph loader
        let subgraph_loader =
            |_uri: &str,
             _base_dir: Option<&str>,
             _new_base_dir: &mut Option<String>|
             -> Result<Graph> { Ok(subgraph.clone()) };

        // Flatten the graph
        let flattened =
            main_graph.flatten_graph(&subgraph_loader, None).unwrap().unwrap();

        // Verify results
        assert_eq!(flattened.nodes.len(), 3); // ext_a + 2 from subgraph

        // Check that connections are resolved correctly
        let connections = flattened.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 2);

        // Check that subgraph reference in destination is resolved to ext_d
        let connection_to_subgraph = connections
            .iter()
            .find(|conn| conn.loc.extension.as_deref() == Some("ext_a"))
            .unwrap();
        let cmd_flow = &connection_to_subgraph.cmd.as_ref().unwrap()[0];
        assert_eq!(cmd_flow.name, "B");
        assert_eq!(
            cmd_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_2_ext_d"
        );
        assert!(cmd_flow.dest[0].loc.subgraph.is_none());

        // Check that subgraph reference in source is resolved to ext_c
        let connection_from_subgraph = connections
            .iter()
            .find(|conn| {
                conn.loc.extension.as_deref() == Some("subgraph_2_ext_c")
            })
            .unwrap();
        let cmd_flow = &connection_from_subgraph.cmd.as_ref().unwrap()[0];
        assert_eq!(cmd_flow.name, "H");
        assert_eq!(cmd_flow.dest[0].loc.extension.as_ref().unwrap(), "ext_a");
    }

    #[test]
    fn test_flatten_subgraph_field_reference_missing_exposed_message() {
        // Create a main graph with subgraph field reference
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
                    name: "subgraph_2".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph2.json".to_string(),
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
                    name: "NonExistentCmd".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: None,
                            subgraph: Some("subgraph_2".to_string()),
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

        // Create a subgraph with exposed_messages that doesn't include the
        // requested message
        let subgraph = Graph {
            nodes: vec![GraphNode {
                type_: GraphNodeType::Extension,
                name: "ext_d".to_string(),
                addon: Some("addon_d".to_string()),
                extension_group: None,
                app: None,
                property: None,
                source_uri: None,
            }],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdIn,
                name: "TestCmd".to_string(),
                extension: None,
                subgraph: Some("subgraph_2".to_string()),
            }]),
            exposed_properties: None,
        };

        // Mock subgraph loader
        let subgraph_loader =
            |_uri: &str,
             _base_dir: Option<&str>,
             _new_base_dir: &mut Option<String>|
             -> Result<Graph> { Ok(subgraph.clone()) };

        // Flatten the graph - should fail
        let result = main_graph.flatten_graph(&subgraph_loader, None);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains(
            "Message 'NonExistentCmd' of type 'CmdIn' is not exposed by \
             subgraph 'subgraph_2'"
        ));
    }

    #[test]
    fn test_flatten_subgraph_field_reference_no_exposed_messages() {
        // Create a main graph with subgraph field reference
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
                    name: "subgraph_2".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph2.json".to_string(),
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
                            extension: None,
                            subgraph: Some("subgraph_2".to_string()),
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

        // Create a subgraph without exposed_messages
        let subgraph = Graph {
            nodes: vec![GraphNode {
                type_: GraphNodeType::Extension,
                name: "ext_d".to_string(),
                addon: Some("addon_d".to_string()),
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
             -> Result<Graph> { Ok(subgraph.clone()) };

        // Flatten the graph - should fail
        let result = main_graph.flatten_graph(&subgraph_loader, None);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains(
            "Subgraph 'subgraph_2' does not have exposed_messages defined"
        ));
    }

    #[test]
    fn test_flatten_nested_subgraphs() {
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
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph1.json".to_string(),
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
                    name: "TestCmd".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: Some(
                                "subgraph_1_subgraph_2_ext_z".to_string(),
                            ),
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

        // Create a subgraph that contains another subgraph (nested)
        let subgraph_1 = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_x".to_string(),
                    addon: Some("addon_x".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Subgraph,
                    name: "subgraph_2".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph2.json".to_string(),
                    ),
                },
            ],
            connections: Some(vec![GraphConnection {
                loc: connection::GraphLoc {
                    app: None,
                    extension: Some("ext_x".to_string()),
                    subgraph: None,
                },
                cmd: Some(vec![connection::GraphMessageFlow {
                    name: "InternalCmd".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: Some("subgraph_2_ext_z".to_string()),
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

        // Create the innermost subgraph
        let subgraph_2 = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_y".to_string(),
                    addon: Some("addon_y".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_z".to_string(),
                    addon: Some("addon_z".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
            ],
            connections: Some(vec![GraphConnection {
                loc: connection::GraphLoc {
                    app: None,
                    extension: Some("ext_y".to_string()),
                    subgraph: None,
                },
                cmd: Some(vec![connection::GraphMessageFlow {
                    name: "DeepCmd".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: Some("ext_z".to_string()),
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

        let subgraph_loader = |uri: &str,
                               _base_dir: Option<&str>,
                               _new_base_dir: &mut Option<String>|
         -> Result<Graph> {
            match uri {
                "http://example.com/subgraph1.json" => Ok(subgraph_1.clone()),
                "http://example.com/subgraph2.json" => Ok(subgraph_2.clone()),
                _ => Err(anyhow::anyhow!("Unknown URI: {}", uri)),
            }
        };

        // Flatten the graph - should now work with nested subgraphs
        let flattened =
            main_graph.flatten_graph(&subgraph_loader, None).unwrap().unwrap();

        // Verify results
        assert_eq!(flattened.nodes.len(), 4); // ext_a + ext_x + ext_y + ext_z (all flattened)

        // Check that original extension is preserved
        assert!(flattened.nodes.iter().any(|node| node.name == "ext_a"
            && node.addon == Some("addon_a".to_string())));

        // Check that nested subgraph extensions are flattened with proper
        // prefixes
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_ext_x"
                && node.addon == Some("addon_x".to_string())));
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_subgraph_2_ext_y"
                && node.addon == Some("addon_y".to_string())));
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_subgraph_2_ext_z"
                && node.addon == Some("addon_z".to_string())));

        // Check that connections are flattened correctly
        let connections = flattened.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 3); // Original + 2 internal connections

        // Check that the main connection references the deeply nested extension
        let main_connection = connections
            .iter()
            .find(|conn| conn.loc.extension.as_deref() == Some("ext_a"))
            .unwrap();
        let cmd_flow = &main_connection.cmd.as_ref().unwrap()[0];
        assert_eq!(
            cmd_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_1_subgraph_2_ext_z"
        );

        // Check internal connections are preserved with proper prefixes
        let internal_connection_1 = connections
            .iter()
            .find(|conn| {
                conn.loc.extension.as_deref() == Some("subgraph_1_ext_x")
            })
            .unwrap();
        let internal_cmd_flow_1 =
            &internal_connection_1.cmd.as_ref().unwrap()[0];
        assert_eq!(
            internal_cmd_flow_1.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_1_subgraph_2_ext_z"
        );

        let internal_connection_2 = connections
            .iter()
            .find(|conn| {
                conn.loc.extension.as_deref()
                    == Some("subgraph_1_subgraph_2_ext_y")
            })
            .unwrap();
        let internal_cmd_flow_2 =
            &internal_connection_2.cmd.as_ref().unwrap()[0];
        assert_eq!(
            internal_cmd_flow_2.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_1_subgraph_2_ext_z"
        );
    }

    #[test]
    fn test_flatten_nested_subgraphs_with_exposed_messages() {
        // Create a main graph with subgraph field references
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
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph1.json".to_string(),
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
                    name: "TestCmd".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: None,
                            subgraph: Some("subgraph_1".to_string()),
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

        // Create a subgraph that contains another subgraph (nested)
        let subgraph_1 = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_x".to_string(),
                    addon: Some("addon_x".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Subgraph,
                    name: "subgraph_2".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph2.json".to_string(),
                    ),
                },
            ],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdIn,
                name: "TestCmd".to_string(),
                extension: None,
                subgraph: Some("subgraph_2".to_string()),
            }]),
            exposed_properties: None,
        };

        // Create the innermost subgraph
        let subgraph_2 = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_y".to_string(),
                    addon: Some("addon_y".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_z".to_string(),
                    addon: Some("addon_z".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
            ],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdIn,
                subgraph: None,
                name: "TestCmd".to_string(),
                extension: Some("ext_z".to_string()),
            }]),
            exposed_properties: None,
        };

        let subgraph_loader = |uri: &str,
                               _base_dir: Option<&str>,
                               _new_base_dir: &mut Option<String>|
         -> Result<Graph> {
            match uri {
                "http://example.com/subgraph1.json" => Ok(subgraph_1.clone()),
                "http://example.com/subgraph2.json" => Ok(subgraph_2.clone()),
                _ => Err(anyhow::anyhow!("Unknown URI: {}", uri)),
            }
        };

        // Flatten the graph - should work with nested subgraphs and
        // exposed_messages
        let flattened =
            main_graph.flatten_graph(&subgraph_loader, None).unwrap().unwrap();

        // Verify results
        assert_eq!(flattened.nodes.len(), 4); // ext_a + ext_x + ext_y + ext_z (all flattened)

        // Check that nested subgraph extensions are flattened with proper
        // prefixes
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_ext_x"));
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_subgraph_2_ext_y"));
        assert!(flattened
            .nodes
            .iter()
            .any(|node| node.name == "subgraph_1_subgraph_2_ext_z"));

        // Check that connections are resolved correctly through nested
        // exposed_messages
        let connections = flattened.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let main_connection = connections
            .iter()
            .find(|conn| conn.loc.extension.as_deref() == Some("ext_a"))
            .unwrap();
        let cmd_flow = &main_connection.cmd.as_ref().unwrap()[0];
        // The subgraph reference should be resolved to the deeply nested
        // extension
        assert_eq!(
            cmd_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_1_subgraph_2_ext_z"
        );
        assert!(cmd_flow.dest[0].loc.subgraph.is_none());
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

        let subgraph_loader = |_uri: &str,
                               _base_dir: Option<&str>,
                               _new_base_dir: &mut Option<String>|
         -> Result<Graph> {
            unreachable!("Should not be called")
        };

        let result = main_graph.flatten_graph(&subgraph_loader, None);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Subgraph node 'subgraph_1' must have source_uri"));
    }

    #[test]
    fn test_flatten_subgraph_field_reference_all_message_types() {
        // Create a main graph with subgraph field references for all message
        // types
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
                    name: "subgraph_3".to_string(),
                    addon: None,
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph3.json".to_string(),
                    ),
                },
            ],
            connections: Some(vec![
                // ext_a sends various message types to subgraph_3
                GraphConnection {
                    loc: connection::GraphLoc {
                        app: None,
                        extension: Some("ext_a".to_string()),
                        subgraph: None,
                    },
                    cmd: Some(vec![connection::GraphMessageFlow {
                        name: "TestCmd".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: None,
                                subgraph: Some("subgraph_3".to_string()),
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    data: Some(vec![connection::GraphMessageFlow {
                        name: "TestData".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: None,
                                subgraph: Some("subgraph_3".to_string()),
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    audio_frame: Some(vec![connection::GraphMessageFlow {
                        name: "TestAudio".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: None,
                                subgraph: Some("subgraph_3".to_string()),
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    video_frame: Some(vec![connection::GraphMessageFlow {
                        name: "TestVideo".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: None,
                                subgraph: Some("subgraph_3".to_string()),
                            },
                            msg_conversion: None,
                        }],
                    }]),
                },
                // subgraph_3 sends various message types to ext_a
                GraphConnection {
                    loc: connection::GraphLoc {
                        app: None,
                        extension: None,
                        subgraph: Some("subgraph_3".to_string()),
                    },
                    cmd: Some(vec![connection::GraphMessageFlow {
                        name: "ResponseCmd".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: Some("ext_a".to_string()),
                                subgraph: None,
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    data: Some(vec![connection::GraphMessageFlow {
                        name: "ResponseData".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: Some("ext_a".to_string()),
                                subgraph: None,
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    audio_frame: Some(vec![connection::GraphMessageFlow {
                        name: "ResponseAudio".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: Some("ext_a".to_string()),
                                subgraph: None,
                            },
                            msg_conversion: None,
                        }],
                    }]),
                    video_frame: Some(vec![connection::GraphMessageFlow {
                        name: "ResponseVideo".to_string(),
                        dest: vec![connection::GraphDestination {
                            loc: connection::GraphLoc {
                                app: None,
                                extension: Some("ext_a".to_string()),
                                subgraph: None,
                            },
                            msg_conversion: None,
                        }],
                    }]),
                },
            ]),
            exposed_messages: None,
            exposed_properties: None,
        };

        // Create a subgraph with exposed_messages for all message types
        let subgraph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_input".to_string(),
                    addon: Some("addon_input".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_output".to_string(),
                    addon: Some("addon_output".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
            ],
            connections: None,
            exposed_messages: Some(vec![
                // Input messages (from external to subgraph)
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdIn,
                    name: "TestCmd".to_string(),
                    extension: Some("ext_input".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::DataIn,
                    name: "TestData".to_string(),
                    extension: Some("ext_input".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::AudioFrameIn,
                    name: "TestAudio".to_string(),
                    extension: Some("ext_input".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::VideoFrameIn,
                    name: "TestVideo".to_string(),
                    extension: Some("ext_input".to_string()),
                    subgraph: None,
                },
                // Output messages (from subgraph to external)
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdOut,
                    name: "ResponseCmd".to_string(),
                    extension: Some("ext_output".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::DataOut,
                    name: "ResponseData".to_string(),
                    extension: Some("ext_output".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::AudioFrameOut,
                    name: "ResponseAudio".to_string(),
                    extension: Some("ext_output".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::VideoFrameOut,
                    name: "ResponseVideo".to_string(),
                    extension: Some("ext_output".to_string()),
                    subgraph: None,
                },
            ]),
            exposed_properties: None,
        };

        // Mock subgraph loader
        let subgraph_loader =
            |_uri: &str,
             _base_dir: Option<&str>,
             _new_base_dir: &mut Option<String>|
             -> Result<Graph> { Ok(subgraph.clone()) };

        // Flatten the graph
        let flattened =
            main_graph.flatten_graph(&subgraph_loader, None).unwrap().unwrap();

        // Verify results
        assert_eq!(flattened.nodes.len(), 3); // ext_a + 2 from subgraph

        // Check that connections are resolved correctly
        let connections = flattened.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 2); // 1 original + 1 grouped connection from subgraph source

        // Check that subgraph references in destinations are resolved correctly
        let connection_to_subgraph = connections
            .iter()
            .find(|conn| conn.loc.extension.as_deref() == Some("ext_a"))
            .unwrap();

        // Verify cmd destination
        let cmd_flow = &connection_to_subgraph.cmd.as_ref().unwrap()[0];
        assert_eq!(cmd_flow.name, "TestCmd");
        assert_eq!(
            cmd_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_3_ext_input"
        );
        assert!(cmd_flow.dest[0].loc.subgraph.is_none());

        // Verify data destination
        let data_flow = &connection_to_subgraph.data.as_ref().unwrap()[0];
        assert_eq!(data_flow.name, "TestData");
        assert_eq!(
            data_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_3_ext_input"
        );

        // Verify audio_frame destination
        let audio_flow =
            &connection_to_subgraph.audio_frame.as_ref().unwrap()[0];
        assert_eq!(audio_flow.name, "TestAudio");
        assert_eq!(
            audio_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_3_ext_input"
        );

        // Verify video_frame destination
        let video_flow =
            &connection_to_subgraph.video_frame.as_ref().unwrap()[0];
        assert_eq!(video_flow.name, "TestVideo");
        assert_eq!(
            video_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_3_ext_input"
        );

        // Check that subgraph references in sources are resolved correctly
        // Now we should have 1 grouped connection with all message types from
        // the same extension

        // Find the grouped connection from subgraph
        let grouped_connection = connections
            .iter()
            .find(|conn| {
                conn.loc.extension.as_deref() == Some("subgraph_3_ext_output")
            })
            .unwrap();

        // Verify all message types are present in the same connection
        assert!(grouped_connection.cmd.is_some());
        assert!(grouped_connection.data.is_some());
        assert!(grouped_connection.audio_frame.is_some());
        assert!(grouped_connection.video_frame.is_some());

        // Verify cmd flow
        let cmd_flow = &grouped_connection.cmd.as_ref().unwrap()[0];
        assert_eq!(cmd_flow.name, "ResponseCmd");
        assert_eq!(cmd_flow.dest[0].loc.extension.as_ref().unwrap(), "ext_a");

        // Verify data flow
        let data_flow = &grouped_connection.data.as_ref().unwrap()[0];
        assert_eq!(data_flow.name, "ResponseData");
        assert_eq!(data_flow.dest[0].loc.extension.as_ref().unwrap(), "ext_a");

        // Verify audio_frame flow
        let audio_flow = &grouped_connection.audio_frame.as_ref().unwrap()[0];
        assert_eq!(audio_flow.name, "ResponseAudio");
        assert_eq!(audio_flow.dest[0].loc.extension.as_ref().unwrap(), "ext_a");

        // Verify video_frame flow
        let video_flow = &grouped_connection.video_frame.as_ref().unwrap()[0];
        assert_eq!(video_flow.name, "ResponseVideo");
        assert_eq!(video_flow.dest[0].loc.extension.as_ref().unwrap(), "ext_a");
    }

    #[test]
    fn test_flatten_subgraph_field_reference_exposed_properties() {
        // Create a main graph with subgraph field references in
        // exposed_properties
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
                    property: None,
                    source_uri: Some(
                        "http://example.com/subgraph1.json".to_string(),
                    ),
                },
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: Some(vec![
                // Extension-based exposed property
                GraphExposedProperty {
                    extension: Some("ext_a".to_string()),
                    name: "config_a".to_string(),
                    subgraph: None,
                },
                // Subgraph-based exposed property
                GraphExposedProperty {
                    extension: None,
                    name: "config_b".to_string(),
                    subgraph: Some("subgraph_1".to_string()),
                },
            ]),
        };

        // Create a subgraph with exposed_properties
        let subgraph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_x".to_string(),
                    addon: Some("addon_x".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "ext_y".to_string(),
                    addon: Some("addon_y".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    source_uri: None,
                },
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: Some(vec![GraphExposedProperty {
                extension: Some("ext_y".to_string()),
                name: "config_b".to_string(),
                subgraph: None,
            }]),
        };

        // Mock subgraph loader
        let subgraph_loader =
            |_uri: &str,
             _base_dir: Option<&str>,
             _new_base_dir: &mut Option<String>|
             -> Result<Graph> { Ok(subgraph.clone()) };

        // Flatten the graph with preserve_exposed_info = true
        let flattened =
            Graph::flatten(&main_graph, &subgraph_loader, None, true)
                .unwrap()
                .unwrap();

        // Verify results
        assert_eq!(flattened.nodes.len(), 3); // ext_a + 2 from subgraph

        // Check that exposed_properties are updated correctly
        let exposed_properties = flattened.exposed_properties.as_ref().unwrap();
        assert_eq!(exposed_properties.len(), 2);

        // Check that extension-based exposed property is preserved
        let ext_a_property = exposed_properties
            .iter()
            .find(|prop| prop.extension.as_deref() == Some("ext_a"))
            .unwrap();
        assert_eq!(ext_a_property.name, "config_a");
        assert!(ext_a_property.subgraph.is_none());

        // Check that subgraph-based exposed property is expanded
        let expanded_property = exposed_properties
            .iter()
            .find(|prop| prop.extension.as_deref() == Some("subgraph_1_ext_y"))
            .unwrap();
        assert_eq!(expanded_property.name, "config_b");
        assert!(expanded_property.subgraph.is_none());
    }

    #[test]
    fn test_flatten_with_load_graph_from_uri_as_subgraph_loader() {
        use std::fs;
        use tempfile::tempdir;

        // Create a temporary directory and subgraph file
        let temp_dir = tempdir().unwrap();
        let subgraph_file_path = temp_dir.path().join("test_subgraph.json");

        // Define a test subgraph
        let subgraph_json = r#"
        {
            "nodes": [
                {
                    "type": "extension",
                    "name": "ext_c",
                    "addon": "addon_c",
                    "extension_group": "test_group"
                },
                {
                    "type": "extension",
                    "name": "ext_d",
                    "addon": "addon_d",
                    "extension_group": "test_group"
                }
            ],
            "connections": [
                {
                    "extension": "ext_c",
                    "cmd": [
                        {
                            "name": "internal_cmd",
                            "dest": [
                                {
                                    "extension": "ext_d"
                                }
                            ]
                        }
                    ]
                }
            ]
        }
        "#;

        // Write the subgraph to the file
        fs::write(&subgraph_file_path, subgraph_json).unwrap();

        // Create a main graph that references the subgraph file
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
                    property: None,
                    source_uri: Some(
                        subgraph_file_path.to_str().unwrap().to_string(),
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
                    name: "test_cmd".to_string(),
                    dest: vec![connection::GraphDestination {
                        loc: connection::GraphLoc {
                            app: None,
                            extension: Some("subgraph_1_ext_c".to_string()),
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

        // Use load_graph_from_uri_with_base_dir as the subgraph_loader
        let base_dir = temp_dir.path().to_str().unwrap();
        let subgraph_loader = |uri: &str,
                               base_dir_param: Option<&str>,
                               new_base_dir: &mut Option<String>|
         -> Result<Graph> {
            // For this test, we'll use the provided base_dir_param if
            // available, otherwise fall back to the test's base_dir
            let effective_base_dir = base_dir_param.or(Some(base_dir));
            load_graph_from_uri(uri, effective_base_dir, new_base_dir)
        };

        // Flatten the graph
        let flattened =
            main_graph.flatten_graph(&subgraph_loader, None).unwrap().unwrap();

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

        // Check that connections are flattened
        let connections = flattened.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 2); // Original + internal subgraph connection

        // Check that the connection destination is correct
        let main_connection = connections
            .iter()
            .find(|conn| conn.loc.extension.as_deref() == Some("ext_a"))
            .unwrap();
        let cmd_flow = &main_connection.cmd.as_ref().unwrap()[0];
        assert_eq!(
            cmd_flow.dest[0].loc.extension.as_ref().unwrap(),
            "subgraph_1_ext_c"
        );

        // Check internal subgraph connection is preserved
        let internal_connection = connections
            .iter()
            .find(|conn| {
                conn.loc.extension.as_deref() == Some("subgraph_1_ext_c")
            })
            .unwrap();
        assert!(internal_connection.cmd.is_some());
    }
}
