//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::{
        connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow},
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
                    Some("B".to_string()),
                    None,
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
        assert_eq!(exposed_messages[0].msg_type, GraphExposedMessageType::CmdIn);
        assert_eq!(exposed_messages[0].name, "B");
        assert_eq!(exposed_messages[0].extension, Some("ext_d".to_string()));
        assert_eq!(exposed_messages[1].msg_type, GraphExposedMessageType::CmdOut);
        assert_eq!(exposed_messages[1].name, "C");
        assert_eq!(exposed_messages[1].extension, Some("ext_c".to_string()));
        assert_eq!(exposed_messages[2].msg_type, GraphExposedMessageType::DataIn);
        assert_eq!(exposed_messages[2].name, "DataX");
        assert_eq!(exposed_messages[2].extension, Some("ext_d".to_string()));
    }

    #[test]
    fn test_inject_graph_proxy_with_cmd_in() {
        // Create a graph with cmd_in exposed message.
        let graph = Graph {
            nodes: vec![GraphNode::new_extension_node(
                "function_entry".to_string(),
                "function_entry_addon".to_string(),
                Some("some_group".to_string()),
                None,
                None,
            )],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdIn,
                name: "function_call".to_string(),
                extension: Some("function_entry".to_string()),
                subgraph: None,
            }]),
            exposed_properties: None,
        };

        // Inject graph_proxy
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_ok());

        let new_graph = result.unwrap();
        assert!(new_graph.is_some());

        let new_graph = new_graph.unwrap();

        // Verify that ten:graph_proxy node was added
        assert_eq!(new_graph.nodes.len(), 2);
        assert!(new_graph.nodes.iter().any(|node| node.get_name() == "ten:graph_proxy"));

        // Verify connections were created
        let connections = new_graph.connections.unwrap();
        assert!(!connections.is_empty());

        // Find the connection from ten:graph_proxy
        let proxy_conn = connections
            .iter()
            .find(|conn| conn.loc.extension == Some("ten:graph_proxy".to_string()))
            .expect("Connection from ten:graph_proxy should exist");

        // Verify cmd connection exists
        assert!(proxy_conn.cmd.is_some());
        let cmd_flows = proxy_conn.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 1);
        assert_eq!(cmd_flows[0].name, Some("function_call".to_string()));

        // Verify destination
        assert_eq!(cmd_flows[0].dest.len(), 1);
        assert_eq!(cmd_flows[0].dest[0].loc.extension, Some("function_entry".to_string()));
    }

    #[test]
    fn test_inject_graph_proxy_with_cmd_out() {
        // Create a graph with cmd_out exposed message.
        let graph = Graph {
            nodes: vec![GraphNode::new_extension_node(
                "tts".to_string(),
                "tts_addon".to_string(),
                Some("some_group".to_string()),
                None,
                None,
            )],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdOut,
                name: "tts_complete".to_string(),
                extension: Some("tts".to_string()),
                subgraph: None,
            }]),
            exposed_properties: None,
        };

        // Inject graph_proxy
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_ok());

        let new_graph = result.unwrap();
        assert!(new_graph.is_some());

        let new_graph = new_graph.unwrap();

        // Verify that ten:graph_proxy node was added
        assert_eq!(new_graph.nodes.len(), 2);

        // Verify connections were created
        let connections = new_graph.connections.unwrap();
        assert!(!connections.is_empty());

        // Find the connection from tts
        let tts_conn = connections
            .iter()
            .find(|conn| conn.loc.extension == Some("tts".to_string()))
            .expect("Connection from tts should exist");

        // Verify cmd connection exists
        assert!(tts_conn.cmd.is_some());
        let cmd_flows = tts_conn.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 1);
        assert_eq!(cmd_flows[0].name, Some("tts_complete".to_string()));

        // Verify destination is ten:graph_proxy
        assert_eq!(cmd_flows[0].dest.len(), 1);
        assert_eq!(cmd_flows[0].dest[0].loc.extension, Some("ten:graph_proxy".to_string()));
    }

    #[test]
    fn test_inject_graph_proxy_with_multiple_message_types() {
        // Create a graph with multiple message types.
        let graph = Graph {
            nodes: vec![
                GraphNode::new_extension_node(
                    "function_entry".to_string(),
                    "function_entry_addon".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
                GraphNode::new_extension_node(
                    "tts".to_string(),
                    "tts_addon".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
            ],
            connections: None,
            exposed_messages: Some(vec![
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdIn,
                    name: "function_call".to_string(),
                    extension: Some("function_entry".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::AudioFrameOut,
                    name: "pcm_frame".to_string(),
                    extension: Some("tts".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::DataIn,
                    name: "input_data".to_string(),
                    extension: Some("function_entry".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::VideoFrameOut,
                    name: "video_output".to_string(),
                    extension: Some("tts".to_string()),
                    subgraph: None,
                },
            ]),
            exposed_properties: None,
        };

        // Inject graph_proxy
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_ok());

        let new_graph = result.unwrap();
        assert!(new_graph.is_some());

        let new_graph = new_graph.unwrap();

        // Verify that ten:graph_proxy node was added
        assert_eq!(new_graph.nodes.len(), 3);

        // Verify connections were created
        let connections = new_graph.connections.unwrap();
        assert_eq!(connections.len(), 2);

        // Find the connection from ten:graph_proxy (for *_in messages)
        let proxy_conn = connections
            .iter()
            .find(|conn| conn.loc.extension == Some("ten:graph_proxy".to_string()))
            .expect("Connection from ten:graph_proxy should exist");

        // Verify cmd_in and data_in connections
        assert!(proxy_conn.cmd.is_some());
        assert!(proxy_conn.data.is_some());
        assert_eq!(proxy_conn.cmd.as_ref().unwrap().len(), 1);
        assert_eq!(proxy_conn.data.as_ref().unwrap().len(), 1);

        // Find the connection from tts (for *_out messages)
        let tts_conn = connections
            .iter()
            .find(|conn| conn.loc.extension == Some("tts".to_string()))
            .expect("Connection from tts should exist");

        // Verify audio_frame_out and video_frame_out connections
        assert!(tts_conn.audio_frame.is_some());
        assert!(tts_conn.video_frame.is_some());
        assert_eq!(tts_conn.audio_frame.as_ref().unwrap().len(), 1);
        assert_eq!(tts_conn.video_frame.as_ref().unwrap().len(), 1);
    }

    #[test]
    fn test_inject_graph_proxy_with_host_loc_property() {
        // Create a graph with exposed message.
        let graph = Graph {
            nodes: vec![GraphNode::new_extension_node(
                "function_entry".to_string(),
                "function_entry_addon".to_string(),
                Some("some_group".to_string()),
                None,
                None,
            )],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdIn,
                name: "function_call".to_string(),
                extension: Some("function_entry".to_string()),
                subgraph: None,
            }]),
            exposed_properties: None,
        };

        // Create host_loc property
        let host_loc = serde_json::json!({
            "app": "http://localhost:8000",
            "graph": "parent_graph",
            "extension": "caller_extension"
        });

        // Inject graph_proxy with host_loc
        let result = graph.inject_graph_proxy_from_exposed_messages(Some(&host_loc.to_string()));
        assert!(result.is_ok());

        let new_graph = result.unwrap();
        assert!(new_graph.is_some());

        let new_graph = new_graph.unwrap();

        // Find the ten:graph_proxy node
        let proxy_node = new_graph
            .nodes
            .iter()
            .find(|node| node.get_name() == "ten:graph_proxy")
            .expect("ten:graph_proxy node should exist");

        // Verify the property was set
        if let ten_rust::graph::node::GraphNode::Extension {
            content,
        } = proxy_node
        {
            assert!(content.property.is_some());
            let property = content.property.as_ref().unwrap();
            assert!(property["host_loc"].is_object());
            assert_eq!(property["host_loc"]["app"].as_str().unwrap(), "http://localhost:8000");
        } else {
            panic!("ten:graph_proxy should be an extension node");
        }
    }

    #[test]
    fn test_inject_graph_proxy_with_no_exposed_messages() {
        // Create a graph without exposed messages.
        let graph = Graph {
            nodes: vec![GraphNode::new_extension_node(
                "ext_a".to_string(),
                "ext_a_addon".to_string(),
                Some("some_group".to_string()),
                None,
                None,
            )],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Inject graph_proxy should return None
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_ok());
        assert!(result.unwrap().is_none());
    }

    #[test]
    fn test_inject_graph_proxy_with_empty_exposed_messages() {
        // Create a graph with empty exposed messages.
        let graph = Graph {
            nodes: vec![GraphNode::new_extension_node(
                "ext_a".to_string(),
                "ext_a_addon".to_string(),
                Some("some_group".to_string()),
                None,
                None,
            )],
            connections: None,
            exposed_messages: Some(vec![]),
            exposed_properties: None,
        };

        // Inject graph_proxy should return None
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_ok());
        assert!(result.unwrap().is_none());
    }

    #[test]
    fn test_inject_graph_proxy_extension_not_found() {
        // Create a graph with exposed message referencing non-existent extension.
        let graph = Graph {
            nodes: vec![GraphNode::new_extension_node(
                "ext_a".to_string(),
                "ext_a_addon".to_string(),
                Some("some_group".to_string()),
                None,
                None,
            )],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdIn,
                name: "some_cmd".to_string(),
                extension: Some("non_existent_ext".to_string()),
                subgraph: None,
            }]),
            exposed_properties: None,
        };

        // Inject graph_proxy should fail
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_err());
        let err_msg = result.unwrap_err().to_string();
        assert!(err_msg.contains("non_existent_ext"));
        assert!(err_msg.contains("does not exist"));
    }

    #[test]
    fn test_inject_graph_proxy_already_exists() {
        // Create a graph that already has a node named ten:graph_proxy.
        let graph = Graph {
            nodes: vec![
                GraphNode::new_extension_node(
                    "ten:graph_proxy".to_string(),
                    "some_addon".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
                GraphNode::new_extension_node(
                    "function_entry".to_string(),
                    "function_entry_addon".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
            ],
            connections: None,
            exposed_messages: Some(vec![GraphExposedMessage {
                msg_type: GraphExposedMessageType::CmdIn,
                name: "function_call".to_string(),
                extension: Some("function_entry".to_string()),
                subgraph: None,
            }]),
            exposed_properties: None,
        };

        // Inject graph_proxy should fail
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_err());
        let err_msg = result.unwrap_err().to_string();
        assert!(err_msg.contains("ten:graph_proxy"));
        assert!(err_msg.contains("already contains"));
    }

    #[test]
    fn test_inject_graph_proxy_with_same_msg_name_different_extensions() {
        // Create a graph with multiple extensions exposing the same message name.
        // This tests the fix for the bug where same msg name from different
        // extensions would overwrite each other instead of being added to the same
        // flow's dest vector.
        let graph = Graph {
            nodes: vec![
                GraphNode::new_extension_node(
                    "ext_1".to_string(),
                    "ext_1_addon".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
                GraphNode::new_extension_node(
                    "ext_2".to_string(),
                    "ext_2_addon".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
                GraphNode::new_extension_node(
                    "ext_3".to_string(),
                    "ext_3_addon".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
            ],
            connections: None,
            exposed_messages: Some(vec![
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdIn,
                    name: "shared_cmd".to_string(),
                    extension: Some("ext_1".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::CmdIn,
                    name: "shared_cmd".to_string(),
                    extension: Some("ext_2".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::DataOut,
                    name: "shared_data".to_string(),
                    extension: Some("ext_1".to_string()),
                    subgraph: None,
                },
                GraphExposedMessage {
                    msg_type: GraphExposedMessageType::DataOut,
                    name: "shared_data".to_string(),
                    extension: Some("ext_3".to_string()),
                    subgraph: None,
                },
            ]),
            exposed_properties: None,
        };

        // Inject graph_proxy
        let result = graph.inject_graph_proxy_from_exposed_messages(None);
        assert!(result.is_ok());

        let new_graph = result.unwrap();
        assert!(new_graph.is_some());

        let new_graph = new_graph.unwrap();

        // Verify that ten:graph_proxy node was added
        assert_eq!(new_graph.nodes.len(), 4);

        // Verify connections were created
        let connections = new_graph.connections.as_ref().unwrap();

        // Find the connection from ten:graph_proxy (for CmdIn messages)
        let proxy_conn = connections
            .iter()
            .find(|conn| conn.loc.extension == Some("ten:graph_proxy".to_string()))
            .expect("Connection from ten:graph_proxy should exist");

        // Verify cmd connection exists with only ONE flow for "shared_cmd"
        assert!(proxy_conn.cmd.is_some());
        let cmd_flows = proxy_conn.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 1, "There should be only one flow for the shared message name");

        // Verify the flow has the correct name
        assert_eq!(cmd_flows[0].name, Some("shared_cmd".to_string()));

        // Verify the flow has TWO destinations (ext_1 and ext_2)
        assert_eq!(
            cmd_flows[0].dest.len(),
            2,
            "The flow should have two destinations for ext_1 and ext_2"
        );

        // Verify the destinations are ext_1 and ext_2
        let dest_extensions: Vec<_> =
            cmd_flows[0].dest.iter().map(|d| d.loc.extension.as_ref().unwrap().as_str()).collect();
        assert!(dest_extensions.contains(&"ext_1"));
        assert!(dest_extensions.contains(&"ext_2"));

        // Find connections from ext_1 (for DataOut messages)
        let ext1_conn = connections
            .iter()
            .find(|conn| conn.loc.extension == Some("ext_1".to_string()))
            .expect("Connection from ext_1 should exist");

        // Verify data connection exists with only ONE flow for "shared_data"
        assert!(ext1_conn.data.is_some());
        let data_flows = ext1_conn.data.as_ref().unwrap();
        assert_eq!(
            data_flows.len(),
            1,
            "There should be only one flow for the shared message name"
        );

        // Verify the flow has the correct name
        assert_eq!(data_flows[0].name, Some("shared_data".to_string()));

        // Verify the flow has one destination (ten:graph_proxy)
        assert_eq!(data_flows[0].dest.len(), 1);
        assert_eq!(data_flows[0].dest[0].loc.extension, Some("ten:graph_proxy".to_string()));

        // Find connections from ext_3 (for DataOut messages)
        let ext3_conn = connections
            .iter()
            .find(|conn| conn.loc.extension == Some("ext_3".to_string()))
            .expect("Connection from ext_3 should exist");

        // Verify data connection exists with only ONE flow for "shared_data"
        assert!(ext3_conn.data.is_some());
        let data_flows_ext3 = ext3_conn.data.as_ref().unwrap();
        assert_eq!(
            data_flows_ext3.len(),
            1,
            "There should be only one flow for the shared message name"
        );

        // Verify the flow has the correct name
        assert_eq!(data_flows_ext3[0].name, Some("shared_data".to_string()));

        // Verify the flow has one destination (ten:graph_proxy)
        assert_eq!(data_flows_ext3[0].dest.len(), 1);
        assert_eq!(data_flows_ext3[0].dest[0].loc.extension, Some("ten:graph_proxy".to_string()));
    }
}
