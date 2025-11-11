//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::graph_info::GraphContent;

    #[tokio::test]
    async fn test_graph_with_selector() {
        let mut graph_content = serde_json::from_str::<GraphContent>(include_str!(
            "../../test_data/graph_with_selector/graph_with_selector_1.json"
        ))
        .unwrap();

        graph_content.validate_and_complete_and_flatten(None).await.unwrap();

        let graph = graph_content.flattened_graph.as_ref().unwrap();

        // test_extension_1,2,3 --data--> test_extension_4
        // test_extension_3     --cmd --> test_extension_1,2
        // ----merged---
        // test_extension_1     --data--> test_extension_4
        // test_extension_2     --data--> test_extension_4
        // test_extension_3     --cmd --> test_extension_1,2
        //                      --data--> test_extension_4

        assert_eq!(graph.connections.as_ref().unwrap().len(), 3);

        // Get the connection of test_extension_1
        let connection = graph
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("test_extension_1".to_string()))
            .unwrap();

        assert!(connection.data.is_some());
        let data = connection.data.as_ref().unwrap();
        assert_eq!(data.len(), 1);
        assert_eq!(data[0].name.as_deref(), Some("hi"));
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(data[0].dest[0].loc.extension, Some("test_extension_4".to_string()));

        let connection = graph
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("test_extension_2".to_string()))
            .unwrap();
        assert!(connection.data.is_some());
        let data = connection.data.as_ref().unwrap();
        assert_eq!(data.len(), 1);
        assert_eq!(data[0].name.as_deref(), Some("hi"));
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(data[0].dest[0].loc.extension, Some("test_extension_4".to_string()));

        let connection = graph
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("test_extension_3".to_string()))
            .unwrap();

        assert!(connection.cmd.is_some());
        let cmd = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd.len(), 1);
        assert_eq!(cmd[0].name.as_deref(), Some("hello_world"));
        assert_eq!(cmd[0].dest.len(), 2);
        assert!(cmd[0]
            .dest
            .iter()
            .any(|d| d.loc.extension == Some("test_extension_1".to_string())));
        assert!(cmd[0]
            .dest
            .iter()
            .any(|d| d.loc.extension == Some("test_extension_2".to_string())));

        assert!(connection.data.is_some());
        let data = connection.data.as_ref().unwrap();
        assert_eq!(data.len(), 1);
        assert_eq!(data[0].name.as_deref(), Some("hi"));
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(data[0].dest[0].loc.extension, Some("test_extension_4".to_string()));
    }

    #[tokio::test]
    async fn test_get_nodes_by_selector_node_name() {
        use ten_rust::graph::Graph;

        let graph_str =
            include_str!("../../test_data/graph_with_selector/graph_with_selector_1.json");
        let graph = serde_json::from_str::<Graph>(graph_str).unwrap();
        let nodes = graph.get_nodes_by_selector_node_name("selector_for_ext_1_and_2").unwrap();
        assert_eq!(nodes.len(), 2);
        assert_eq!(nodes[0].get_name(), "test_extension_1");
        assert_eq!(nodes[1].get_name(), "test_extension_2");

        let nodes =
            graph.get_nodes_by_selector_node_name("selector_for_ext_1_and_2_and_3").unwrap();
        assert_eq!(nodes.len(), 3);
        assert_eq!(nodes[0].get_name(), "test_extension_1");
        assert_eq!(nodes[1].get_name(), "test_extension_2");
        assert_eq!(nodes[2].get_name(), "test_extension_3");

        let nodes = graph.get_nodes_by_selector_node_name("selector_for_ext_1_or_3").unwrap();
        assert_eq!(nodes.len(), 2);
        assert_eq!(nodes[0].get_name(), "test_extension_1");
        assert_eq!(nodes[1].get_name(), "test_extension_3");
    }

    #[tokio::test]
    async fn test_populate_selector_message_info() {
        use ten_rust::{
            graph::{node::SelectorMessageInfo, Graph},
            pkg_info::message::{MsgDirection, MsgType},
        };

        let graph_str = include_str!(
            "../../test_data/graph_with_selector/graph_for_populate_selector_message_info.json"
        );
        let mut graph = serde_json::from_str::<Graph>(graph_str).unwrap();

        // Before populating, selectors should have empty messages
        let selector_1 = graph.get_nodes_by_selector_node_name("selector_1_and_2").unwrap();
        assert_eq!(selector_1.len(), 2);

        let selector_all = graph.get_nodes_by_selector_node_name("selector_all").unwrap();
        assert_eq!(selector_all.len(), 3);

        // Populate selector message info
        graph.populate_selector_message_info().unwrap();

        // Verify selector_1_and_2 (matches ext_1 and ext_2)
        // ext_1 has: cmd (command_a, command_b) Out, data (data_x) Out
        // ext_2 has: data (data_y) Out, audio_frame (audio_stream_1) Out
        // Combined: 5 messages all with Out direction
        let selector_1_node = graph
            .nodes
            .iter()
            .find(|node| {
                if let ten_rust::graph::node::GraphNode::Selector {
                    content,
                } = node
                {
                    content.name == "selector_1_and_2"
                } else {
                    false
                }
            })
            .unwrap();

        if let ten_rust::graph::node::GraphNode::Selector {
            content,
        } = selector_1_node
        {
            // Check we have 5 messages
            assert_eq!(content.messages.len(), 5, "Expected 5 messages for selector_1_and_2");

            // Verify all messages are Out direction
            for msg in &content.messages {
                assert_eq!(
                    msg.direction,
                    MsgDirection::Out,
                    "All messages should be Out direction"
                );
            }

            // Check specific messages exist
            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Cmd
                    && m.msg_name == "command_a"
                    && m.direction == MsgDirection::Out),
                "Missing command_a"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Cmd
                    && m.msg_name == "command_b"
                    && m.direction == MsgDirection::Out),
                "Missing command_b"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Data
                    && m.msg_name == "data_x"
                    && m.direction == MsgDirection::Out),
                "Missing data_x"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Data
                    && m.msg_name == "data_y"
                    && m.direction == MsgDirection::Out),
                "Missing data_y"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::AudioFrame
                    && m.msg_name == "audio_stream_1"
                    && m.direction == MsgDirection::Out),
                "Missing audio_stream_1"
            );
        } else {
            panic!("Expected selector node");
        }

        // Verify selector_all (matches ext_1, ext_2, and ext_3)
        // ext_1 has: cmd (command_a, command_b) Out, data (data_x) Out
        // ext_2 has: data (data_y) Out, audio_frame (audio_stream_1) Out
        // ext_3 has: video_frame (video_stream_1, video_stream_2) Out, data (data_z)
        // Out Combined: 8 messages all with Out direction
        let selector_all_node = graph
            .nodes
            .iter()
            .find(|node| {
                if let ten_rust::graph::node::GraphNode::Selector {
                    content,
                } = node
                {
                    content.name == "selector_all"
                } else {
                    false
                }
            })
            .unwrap();

        if let ten_rust::graph::node::GraphNode::Selector {
            content,
        } = selector_all_node
        {
            // Check we have 8 messages
            assert_eq!(content.messages.len(), 8, "Expected 8 messages for selector_all");

            // Verify all messages are Out direction
            for msg in &content.messages {
                assert_eq!(
                    msg.direction,
                    MsgDirection::Out,
                    "All messages should be Out direction"
                );
            }

            // Group messages by type for verification
            let cmd_messages: Vec<&SelectorMessageInfo> =
                content.messages.iter().filter(|m| m.msg_type == MsgType::Cmd).collect();
            assert_eq!(cmd_messages.len(), 2, "Expected 2 cmd messages");

            let data_messages: Vec<&SelectorMessageInfo> =
                content.messages.iter().filter(|m| m.msg_type == MsgType::Data).collect();
            assert_eq!(data_messages.len(), 3, "Expected 3 data messages");

            let audio_messages: Vec<&SelectorMessageInfo> =
                content.messages.iter().filter(|m| m.msg_type == MsgType::AudioFrame).collect();
            assert_eq!(audio_messages.len(), 1, "Expected 1 audio message");

            let video_messages: Vec<&SelectorMessageInfo> =
                content.messages.iter().filter(|m| m.msg_type == MsgType::VideoFrame).collect();
            assert_eq!(video_messages.len(), 2, "Expected 2 video messages");

            // Verify specific message names
            let message_names: Vec<String> =
                content.messages.iter().map(|m| m.msg_name.clone()).collect();

            assert!(message_names.contains(&"command_a".to_string()), "Missing command_a");
            assert!(message_names.contains(&"command_b".to_string()), "Missing command_b");
            assert!(message_names.contains(&"data_x".to_string()), "Missing data_x");
            assert!(message_names.contains(&"data_y".to_string()), "Missing data_y");
            assert!(message_names.contains(&"data_z".to_string()), "Missing data_z");
            assert!(
                message_names.contains(&"audio_stream_1".to_string()),
                "Missing audio_stream_1"
            );
            assert!(
                message_names.contains(&"video_stream_1".to_string()),
                "Missing video_stream_1"
            );
            assert!(
                message_names.contains(&"video_stream_2".to_string()),
                "Missing video_stream_2"
            );
        } else {
            panic!("Expected selector node");
        }
    }
}
