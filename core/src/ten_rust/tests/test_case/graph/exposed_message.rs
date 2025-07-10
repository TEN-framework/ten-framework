//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::{
        connection::{
            GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow,
        },
        node::GraphNode,
        Graph, GraphExposedMessage, GraphExposedMessageType,
    };

    #[test]
    fn test_graph_with_exposed_messages_serde() {
        // Create a graph with exposed messages.
        let graph = Graph {
            nodes: vec![
                GraphNode::new_extension_node(
                    "ext_c".to_string(),
                    "extension_c".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
                GraphNode::new_extension_node(
                    "ext_d".to_string(),
                    "extension_d".to_string(),
                    Some("another_group".to_string()),
                    None,
                    None,
                ),
            ],
            connections: Some(vec![GraphConnection {
                loc: GraphLoc {
                    extension: Some("ext_c".to_string()),
                    app: None,
                    subgraph: None,
                    selector: None,
                },
                cmd: Some(vec![GraphMessageFlow::new(
                    "B".to_string(),
                    vec![GraphDestination {
                        loc: GraphLoc {
                            extension: Some("ext_d".to_string()),
                            subgraph: None,
                            app: None,
                            selector: None,
                        },
                        msg_conversion: None,
                    }],
                    vec![],
                )]),
                data: None,
                video_frame: None,
                audio_frame: None,
            }]),
            exposed_messages: Some(vec![
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdIn,
                    name: "B".to_string(),
                    extension: Some("ext_d".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdOut,
                    name: "C".to_string(),
                    extension: Some("ext_c".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::DataIn,
                    name: "DataX".to_string(),
                    extension: Some("ext_d".to_string()),
                    subgraph: None,
                },
            ]),
            exposed_properties: None,
        };

        // Serialize to JSON.
        let json = serde_json::to_string(&graph).unwrap();

        // Deserialize from JSON.
        let deserialized_graph: Graph = serde_json::from_str(&json).unwrap();

        // Verify the deserialized graph.
        assert_eq!(deserialized_graph.nodes.len(), 2);
        assert_eq!(deserialized_graph.nodes[0].get_name(), "ext_c");
        assert_eq!(deserialized_graph.nodes[1].get_name(), "ext_d");

        let connections = deserialized_graph.connections.unwrap();
        assert_eq!(connections.len(), 1);
        assert_eq!(connections[0].loc.extension, Some("ext_c".to_string()));

        let exposed_messages = deserialized_graph.exposed_messages.unwrap();
        assert_eq!(exposed_messages.len(), 3);
        assert_eq!(
            exposed_messages[0].msg_type,
            GraphExposedMessageType::CmdIn
        );
        assert_eq!(exposed_messages[0].name, "B");
        assert_eq!(exposed_messages[0].extension, Some("ext_d".to_string()));
        assert_eq!(
            exposed_messages[1].msg_type,
            GraphExposedMessageType::CmdOut
        );
        assert_eq!(exposed_messages[1].name, "C");
        assert_eq!(exposed_messages[1].extension, Some("ext_c".to_string()));
        assert_eq!(
            exposed_messages[2].msg_type,
            GraphExposedMessageType::DataIn
        );
        assert_eq!(exposed_messages[2].name, "DataX");
        assert_eq!(exposed_messages[2].extension, Some("ext_d".to_string()));
    }
}
