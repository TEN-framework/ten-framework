//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use anyhow::Result;
    use serde_json::Value;
    use tempfile::TempDir;

    use ten_manager::constants::TEST_DIR;
    use ten_manager::graph::connections::add::graph_add_connection;
    use ten_manager::graph::update_graph_connections_all_fields;
    use ten_rust::graph::connection::{
        GraphConnection, GraphDestination, GraphMessageFlow,
    };
    use ten_rust::graph::Graph;
    use ten_rust::pkg_info::constants::PROPERTY_JSON_FILENAME;
    use ten_rust::pkg_info::message::MsgType;

    use crate::test_case::common::mock::inject_all_standard_pkgs_for_mock;
    use crate::test_case::graph::connection::create_test_node;

    #[test]
    fn test_add_connection_1() -> Result<()> {
        // Create a temporary directory for our test.
        let temp_dir = TempDir::new()?;
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // First, create the initial property.json with a connection.
        let initial_json =
            include_str!("../../../test_data/initial_property.json");

        // Expected JSON after adding the connections.
        let expected_json =
            include_str!("../../../test_data/expected_property.json");

        // Write the initial JSON to property.json.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, initial_json)?;

        // Parse the initial JSON to all_fields.
        let mut all_fields: serde_json::Map<String, Value> =
            serde_json::from_str(initial_json)?;

        // Create connections to add.
        let connection1 = GraphConnection {
            app: Some("http://example.com:8000".to_string()),
            extension: "extension_1".to_string(),
            cmd: Some(vec![GraphMessageFlow {
                name: "new_cmd1".to_string(),
                dest: vec![GraphDestination {
                    app: Some("http://example.com:8000".to_string()),
                    extension: "extension_2".to_string(),
                    msg_conversion: None,
                }],
            }]),
            data: None,
            audio_frame: None,
            video_frame: None,
        };

        let connection2 = GraphConnection {
            app: Some("http://example.com:8000".to_string()),
            extension: "extension_1".to_string(),
            cmd: Some(vec![GraphMessageFlow {
                name: "new_cmd2".to_string(),
                dest: vec![GraphDestination {
                    app: Some("http://example.com:8000".to_string()),
                    extension: "extension_3".to_string(),
                    msg_conversion: None,
                }],
            }]),
            data: None,
            audio_frame: None,
            video_frame: None,
        };

        let connections_to_add = vec![connection1, connection2];

        // Update the connections in memory and in the file.
        update_graph_connections_all_fields(
            &test_dir,
            &mut all_fields,
            "test_graph",
            Some(&connections_to_add),
            None,
            None,
        )?;

        // Read the updated property.json.
        let actual_json = std::fs::read_to_string(&property_path)?;

        // Normalize both JSON strings (parse and reformat to remove whitespace
        // differences).
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_json)?;
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_json)?;

        assert_eq!(
            expected_value, actual_value,
            "Updated property does not match expected property"
        );

        Ok(())
    }

    #[test]
    fn test_add_connection_2() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            TEST_DIR,
        );

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node(
                    "ext1",
                    "extension_addon_1",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext2",
                    "extension_addon_2",
                    Some("http://example.com:8000"),
                ),
            ],
            connections: None,
        };

        // Test adding a connection.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "test_cmd".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );

        assert!(result.is_ok());
        assert!(graph.connections.is_some());

        let connections = graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let connection = &connections[0];
        assert_eq!(connection.app, Some("http://example.com:8000".to_string()));
        assert_eq!(connection.extension, "ext1");

        let cmd_flows = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 1);

        let flow = &cmd_flows[0];
        assert_eq!(flow.name, "test_cmd");
        assert_eq!(flow.dest.len(), 1);

        let dest = &flow.dest[0];
        assert_eq!(dest.app, Some("http://example.com:8000".to_string()));
        assert_eq!(dest.extension, "ext2");
    }

    #[test]
    fn test_add_connection_nonexistent_source() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            TEST_DIR,
        );

        // Create a graph with only one node.
        let mut graph = Graph {
            nodes: vec![create_test_node(
                "ext2",
                "extension_addon_2",
                Some("app1"),
            )],
            connections: None,
        };

        // Test adding a connection with nonexistent source.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("app1".to_string()),
            "ext1".to_string(), // This node doesn't exist.
            MsgType::Cmd,
            "test_cmd".to_string(),
            Some("app1".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );

        assert!(result.is_err());
        assert!(graph.connections.is_none()); // Graph should remain unchanged.
    }

    #[test]
    fn test_add_connection_nonexistent_destination() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            TEST_DIR,
        );

        // Create a graph with only one node.
        let mut graph = Graph {
            nodes: vec![create_test_node(
                "ext1",
                "extension_addon_1",
                Some("app1"),
            )],
            connections: None,
        };

        // Test adding a connection with nonexistent destination.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("app1".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "test_cmd".to_string(),
            Some("app1".to_string()),
            "ext2".to_string(), // This node doesn't exist.
            &pkgs_cache,
            None,
        );

        assert!(result.is_err());
        assert!(graph.connections.is_none()); // Graph should remain unchanged.
    }

    #[test]
    fn test_add_connection_to_existing_flow() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            TEST_DIR,
        );

        // Create a graph with three nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node(
                    "ext1",
                    "extension_addon_1",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext2",
                    "extension_addon_2",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext3",
                    "extension_addon_3",
                    Some("http://example.com:8000"),
                ),
            ],
            connections: None,
        };

        // Add first connection.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "test_cmd".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        // Add second connection with same source and message name but different
        // destination.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "test_cmd_2".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext3".to_string(),
            &pkgs_cache,
            None,
        );
        println!("result: {:?}", result);
        assert!(result.is_ok());

        // Verify that we have one connection with one message flow that has two
        // destinations.
        let connections = graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let connection = &connections[0];
        let cmd_flows = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 2);

        let flow = &cmd_flows[0];
        assert_eq!(flow.name, "test_cmd");
        assert_eq!(flow.dest.len(), 1);

        // Verify destinations.
        assert_eq!(flow.dest[0].extension, "ext2");

        let flow = &cmd_flows[1];
        assert_eq!(flow.name, "test_cmd_2");
        assert_eq!(flow.dest.len(), 1);

        // Verify destinations.
        assert_eq!(flow.dest[0].extension, "ext3");
    }

    #[test]
    fn test_add_different_message_types() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            TEST_DIR,
        );

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node(
                    "ext1",
                    "extension_addon_1",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext2",
                    "extension_addon_2",
                    Some("http://example.com:8000"),
                ),
            ],
            connections: None,
        };

        // Add different message types.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "cmd1".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Data,
            "data1".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::AudioFrame,
            "audio1".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::VideoFrame,
            "video1".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        // Verify that we have one connection with different message flows.
        let connection = &graph.connections.as_ref().unwrap()[0];

        assert!(connection.cmd.is_some());
        assert!(connection.data.is_some());
        assert!(connection.audio_frame.is_some());
        assert!(connection.video_frame.is_some());

        assert_eq!(connection.cmd.as_ref().unwrap()[0].name, "cmd1");
        assert_eq!(connection.data.as_ref().unwrap()[0].name, "data1");
        assert_eq!(connection.audio_frame.as_ref().unwrap()[0].name, "audio1");
        assert_eq!(connection.video_frame.as_ref().unwrap()[0].name, "video1");
    }

    #[test]
    fn test_add_duplicate_connection() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            TEST_DIR,
        );

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node(
                    "ext1",
                    "extension_addon_1",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext2",
                    "extension_addon_2",
                    Some("http://example.com:8000"),
                ),
            ],
            connections: None,
        };

        // Add a connection.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "test_cmd".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        // Try to add the same connection again.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "test_cmd".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );

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

    #[test]
    fn test_schema_compatibility_check() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(
            &mut pkgs_cache,
            &mut graphs_cache,
            TEST_DIR,
        );

        // Create a graph with three nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node(
                    "ext1",
                    "extension_addon_1",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext2",
                    "extension_addon_2",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext3",
                    "extension_addon_3",
                    Some("http://example.com:8000"),
                ),
                create_test_node(
                    "ext4",
                    "extension_addon_4",
                    Some("http://example.com:8000"),
                ),
            ],
            connections: None,
        };

        // Test connecting ext1 to ext2 with compatible schema - should succeed.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "cmd1".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext2".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        // Test connecting ext1 to ext3 with compatible schema - should succeed.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Data,
            "data1".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext3".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        // Test connecting ext1 to ext3 with incompatible schema - should fail.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "cmd_incompatible".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext3".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("schema incompatibility"));

        // Test connecting ext1 to ext4 with compatible schema.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "cmd1".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext4".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_ok());

        // Test connecting ext1 to ext4 with incompatible schema - should fail.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Cmd,
            "cmd2".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext4".to_string(),
            &pkgs_cache,
            None,
        );
        println!("result: {:?}", result);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("schema incompatibility"));

        // Test connecting ext1 to ext3 with incompatible schema for data -
        // should fail.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            Some("http://example.com:8000".to_string()),
            "ext1".to_string(),
            MsgType::Data,
            "data_incompatible".to_string(),
            Some("http://example.com:8000".to_string()),
            "ext3".to_string(),
            &pkgs_cache,
            None,
        );
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("schema incompatibility"));
    }
}
