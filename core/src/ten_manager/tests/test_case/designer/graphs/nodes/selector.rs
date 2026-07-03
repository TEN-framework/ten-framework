//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_manager::designer::graphs::{
        nodes::{populate_selector_messages_info, DesignerGraphNode},
        DesignerGraph,
    };
    use ten_rust::{
        graph::Graph,
        pkg_info::message::{MsgDirection, MsgType},
    };

    #[tokio::test]
    async fn test_populate_selector_message_info() {
        let graph_str = include_str!(
            "../../../../test_data/designer/graphs/nodes/selector/\
             graph_for_populate_selector_message_info.json"
        );
        let graph = serde_json::from_str::<Graph>(graph_str).unwrap();

        // Convert to DesignerGraph
        let mut designer_graph = DesignerGraph {
            nodes: graph
                .nodes
                .iter()
                .filter_map(|node| DesignerGraphNode::try_from(node.clone()).ok())
                .collect(),
            connections: graph
                .connections
                .as_ref()
                .map(|conns| conns.iter().map(|conn| conn.clone().into()).collect())
                .unwrap_or_default(),
            exposed_messages: Vec::new(),
            exposed_properties: Vec::new(),
        };

        // Before populating, selectors should have empty messages
        let selector_1 = graph.get_nodes_by_selector_node_name("selector_1_and_2").unwrap();
        assert_eq!(selector_1.len(), 2);

        let selector_all = graph.get_nodes_by_selector_node_name("selector_all").unwrap();
        assert_eq!(selector_all.len(), 3);

        // Populate selector message info
        populate_selector_messages_info(&mut designer_graph, &graph);

        // Verify selector_1_and_2 (matches ext_1 and ext_2)
        // ext_1 has: cmd (command_a, command_b) Out, data (data_x) Out
        // ext_2 has: data (data_y) Out, audio_frame (audio_stream_1) Out
        // Combined: 5 messages all with Out direction
        let selector_1_node = designer_graph
            .nodes
            .iter()
            .find(|node| {
                if let DesignerGraphNode::Selector {
                    content,
                } = node
                {
                    content.name == "selector_1_and_2"
                } else {
                    false
                }
            })
            .unwrap();

        if let DesignerGraphNode::Selector {
            content,
        } = selector_1_node
        {
            // Check we have 5 messages
            assert_eq!(content.messages.len(), 5, "Expected 5 messages for selector_1_and_2");

            // Check specific messages exist
            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Cmd
                    && m.msg_name == "command_a"
                    && m.direction == MsgDirection::Out
                    && m.node_name == "ext_1"),
                "Missing command_a from ext_1"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Cmd
                    && m.msg_name == "command_b"
                    && m.direction == MsgDirection::Out
                    && m.node_name == "ext_1"),
                "Missing command_b from ext_1"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Data
                    && m.msg_name == "data_x"
                    && m.direction == MsgDirection::Out
                    && m.node_name == "ext_1"),
                "Missing data_x from ext_1"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::Data
                    && m.msg_name == "data_y"
                    && m.direction == MsgDirection::Out
                    && m.node_name == "ext_2"),
                "Missing data_y from ext_2"
            );

            assert!(
                content.messages.iter().any(|m| m.msg_type == MsgType::AudioFrame
                    && m.msg_name == "audio_stream_1"
                    && m.direction == MsgDirection::Out
                    && m.node_name == "ext_2"),
                "Missing audio_stream_1 from ext_2"
            );
        } else {
            panic!("Expected selector node");
        }

        // Verify selector_all (matches ext_1, ext_2, and ext_3)
        // ext_1 has: cmd (command_a, command_b) Out, data (data_x) Out
        // ext_2 has: data (data_y) Out, audio_frame (audio_stream_1) Out
        // ext_3 has: video_frame (video_stream_1, video_stream_2) Out, data (data_z)
        // Out Combined: 8 messages all with Out direction
        let selector_all_node = designer_graph
            .nodes
            .iter()
            .find(|node| {
                if let DesignerGraphNode::Selector {
                    content,
                } = node
                {
                    content.name == "selector_all"
                } else {
                    false
                }
            })
            .unwrap();

        if let DesignerGraphNode::Selector {
            content,
        } = selector_all_node
        {
            // Check we have 8 messages
            assert_eq!(content.messages.len(), 8, "Expected 8 messages for selector_all");

            // Verify specific message names
            assert!(
                content
                    .messages
                    .iter()
                    .any(|m| m.msg_name == "command_a" && m.node_name == "ext_1"),
                "Missing command_a from ext_1"
            );
            assert!(
                content
                    .messages
                    .iter()
                    .any(|m| m.msg_name == "command_b" && m.node_name == "ext_1"),
                "Missing command_b from ext_1"
            );
            assert!(
                content.messages.iter().any(|m| m.msg_name == "data_x" && m.node_name == "ext_1"),
                "Missing data_x from ext_1"
            );
            assert!(
                content.messages.iter().any(|m| m.msg_name == "data_y" && m.node_name == "ext_2"),
                "Missing data_y from ext_2"
            );
            assert!(
                content.messages.iter().any(|m| m.msg_name == "data_z" && m.node_name == "ext_3"),
                "Missing data_z from ext_3"
            );
            assert!(
                content
                    .messages
                    .iter()
                    .any(|m| m.msg_name == "audio_stream_1" && m.node_name == "ext_2"),
                "Missing audio_stream_1 from ext_2"
            );
            assert!(
                content
                    .messages
                    .iter()
                    .any(|m| m.msg_name == "video_stream_1" && m.node_name == "ext_3"),
                "Missing video_stream_1 from ext_3"
            );
            assert!(
                content
                    .messages
                    .iter()
                    .any(|m| m.msg_name == "video_stream_2" && m.node_name == "ext_3"),
                "Missing video_stream_2 from ext_3"
            );
        } else {
            panic!("Expected selector node");
        }
    }
}
