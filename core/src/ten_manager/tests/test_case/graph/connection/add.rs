//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use ten_manager::{constants::TEST_DIR, graph::connections::add::graph_add_connection};
    use ten_rust::{
        graph::{connection::GraphLoc, node::GraphNodeType, Graph},
        pkg_info::message::MsgType,
    };

    use crate::test_case::{
        common::mock::inject_all_standard_pkgs_for_mock, graph::connection::create_test_node,
    };

    #[tokio::test]
    async fn test_add_connection() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test adding a connection.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext2".to_string(),
        )
        .unwrap();
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["test_cmd".to_string()],
            None,
        )
        .await;

        assert!(result.is_ok());
        assert!(graph.connections.is_some());

        let connections = graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let connection = &connections[0];
        assert_eq!(connection.loc.app, Some("http://example.com:8000".to_string()));
        assert_eq!(connection.loc.extension, Some("ext1".to_string()));

        let cmd_flows = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 1);

        let flow = &cmd_flows[0];
        assert_eq!(flow.name.as_deref(), Some("test_cmd"));
        assert_eq!(flow.dest.len(), 1);

        let dest = &flow.dest[0];
        assert_eq!(dest.loc.app, Some("http://example.com:8000".to_string()));
        assert_eq!(dest.loc.extension, Some("ext2".to_string()));
    }

    #[tokio::test]
    async fn test_add_connection_nonexistent_source() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with only one node.
        let mut graph = Graph {
            nodes: vec![create_test_node("ext2", "extension_addon_2", Some("app1"))],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test adding a connection with nonexistent source.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("app1".to_string()),
            GraphNodeType::Extension,
            "ext_1".to_string(),
        )
        .unwrap(); // This node doesn't exist.
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("app1".to_string()),
            GraphNodeType::Extension,
            "ext_2".to_string(),
        )
        .unwrap();
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["test_cmd".to_string()],
            None,
        )
        .await;

        assert!(result.is_err());
        assert!(graph.connections.is_none()); // Graph should remain unchanged.
    }

    #[tokio::test]
    async fn test_add_connection_nonexistent_destination() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with only one node.
        let mut graph = Graph {
            nodes: vec![create_test_node("ext1", "extension_addon_1", Some("app1"))],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test adding a connection with nonexistent destination.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("app1".to_string()),
            GraphNodeType::Extension,
            "ext1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("app1".to_string()),
            GraphNodeType::Extension,
            "ext2".to_string(),
        )
        .unwrap(); // This node doesn't exist.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["test_cmd".to_string()],
            None,
        )
        .await;

        assert!(result.is_err());
        assert!(graph.connections.is_none()); // Graph should remain unchanged.
    }

    #[tokio::test]
    async fn test_add_connection_to_existing_flow() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with three nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
                create_test_node("ext3", "extension_addon_3", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Add first connection.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext2".to_string(),
        )
        .unwrap();
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["test_cmd".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        // Add second connection with same source and message name but different
        // destination.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext3".to_string(),
        )
        .unwrap();
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["test_cmd_2".to_string()],
            None,
        )
        .await;
        println!("result: {result:?}");
        assert!(result.is_ok());

        // Verify that we have one connection with one message flow that has two
        // destinations.
        let connections = graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let connection = &connections[0];
        let cmd_flows = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 2);

        let flow = &cmd_flows[0];
        assert_eq!(flow.name.as_deref(), Some("test_cmd"));
        assert_eq!(flow.dest.len(), 1);

        // Verify destinations.
        assert_eq!(flow.dest[0].loc.extension, Some("ext2".to_string()));

        let flow = &cmd_flows[1];
        assert_eq!(flow.name.as_deref(), Some("test_cmd_2"));
        assert_eq!(flow.dest.len(), 1);

        // Verify destinations.
        assert_eq!(flow.dest[0].loc.extension, Some("ext3".to_string()));
    }

    #[tokio::test]
    async fn test_add_different_message_types() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Add different message types.
        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext2".to_string(),
        )
        .unwrap();
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src.clone(),
            dest.clone(),
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src.clone(),
            dest.clone(),
            MsgType::Data,
            vec!["data1".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src.clone(),
            dest.clone(),
            MsgType::AudioFrame,
            vec!["audio1".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::VideoFrame,
            vec!["video1".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        // Verify that we have one connection with different message flows.
        let connection = &graph.connections.as_ref().unwrap()[0];

        assert!(connection.cmd.is_some());
        assert!(connection.data.is_some());
        assert!(connection.audio_frame.is_some());
        assert!(connection.video_frame.is_some());

        assert_eq!(connection.cmd.as_ref().unwrap()[0].name.as_deref(), Some("cmd1"));
        assert_eq!(connection.data.as_ref().unwrap()[0].name.as_deref(), Some("data1"));
        assert_eq!(connection.audio_frame.as_ref().unwrap()[0].name.as_deref(), Some("audio1"));
        assert_eq!(connection.video_frame.as_ref().unwrap()[0].name.as_deref(), Some("video1"));
    }

    #[tokio::test]
    async fn test_add_duplicate_connection() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext2".to_string(),
        )
        .unwrap();

        // Add a connection.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src.clone(),
            dest.clone(),
            MsgType::Cmd,
            vec!["test_cmd".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        // Try to add the same connection again.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["test_cmd".to_string()],
            None,
        )
        .await;

        // This should fail because the connection already exists.
        assert!(result.is_err());

        // The error message should indicate that the connection already exists.
        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg.contains("Connection already exists"));

        // Verify that the graph wasn't changed by the second add attempt.
        let connections = graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let connection = &connections[0];
        let cmd_flows = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 1);

        let flow = &cmd_flows[0];
        assert_eq!(flow.dest.len(), 1);
    }

    #[tokio::test]
    async fn test_schema_compatibility_check() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with three nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
                create_test_node("ext3", "extension_addon_3", Some("http://example.com:8000")),
                create_test_node("ext4", "extension_addon_4", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let ext1 = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext1".to_string(),
        )
        .unwrap();
        let ext2 = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext2".to_string(),
        )
        .unwrap();
        let ext3 = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext3".to_string(),
        )
        .unwrap();
        let ext4 = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext4".to_string(),
        )
        .unwrap();

        // Test connecting ext1 to ext2 with compatible schema - should succeed.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            ext1.clone(),
            ext2.clone(),
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        // Test connecting ext1 to ext3 with compatible schema - should succeed.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            ext1.clone(),
            ext3.clone(),
            MsgType::Data,
            vec!["data1".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        // Test connecting ext1 to ext3 with incompatible schema - should fail.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            ext1.clone(),
            ext3.clone(),
            MsgType::Cmd,
            vec!["cmd_incompatible".to_string()],
            None,
        )
        .await;
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("schema incompatibility"));

        // Test connecting ext1 to ext4 with compatible schema.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            ext1.clone(),
            ext4.clone(),
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            None,
        )
        .await;
        assert!(result.is_ok());

        // Test connecting ext1 to ext4 with incompatible schema - should fail.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            ext1.clone(),
            ext4.clone(),
            MsgType::Cmd,
            vec!["cmd2".to_string()],
            None,
        )
        .await;
        println!("result: {result:?}");
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("schema incompatibility"));

        // Test connecting ext1 to ext3 with incompatible schema for data -
        // should fail.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            ext1.clone(),
            ext3.clone(),
            MsgType::Data,
            vec!["data_incompatible".to_string()],
            None,
        )
        .await;
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("schema incompatibility"));
    }
}
