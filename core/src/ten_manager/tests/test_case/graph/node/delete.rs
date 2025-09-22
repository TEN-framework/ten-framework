//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_manager::designer::graphs::nodes::delete::graph_delete_extension_node;
    use ten_rust::graph::{
        connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow},
        node::GraphNode,
        Graph,
    };

    fn create_test_node(
        name: &str,
        addon: &str,
        app: Option<&str>,
        extension_group: Option<&str>,
    ) -> GraphNode {
        GraphNode::new_extension_node(
            name.to_string(),
            addon.to_string(),
            extension_group.map(|s| s.to_string()),
            app.map(|s| s.to_string()),
            None,
        )
    }

    fn create_test_connection(
        extension: &str,
        app: Option<&str>,
        cmd_name: &str,
        dest_extension: &str,
        dest_app: Option<&str>,
    ) -> GraphConnection {
        let dest = GraphDestination {
            loc: GraphLoc {
                app: dest_app.map(|s| s.to_string()),
                extension: Some(dest_extension.to_string()),
                subgraph: None,
                selector: None,
            },
            msg_conversion: None,
        };

        let message_flow =
            GraphMessageFlow::new(Some(cmd_name.to_string()), None, vec![dest], vec![]);

        GraphConnection {
            loc: GraphLoc {
                app: app.map(|s| s.to_string()),
                extension: Some(extension.to_string()),
                subgraph: None,
                selector: None,
            },
            cmd: Some(vec![message_flow]),
            data: None,
            audio_frame: None,
            video_frame: None,
        }
    }

    #[tokio::test]
    async fn test_delete_extension_node() {
        // Create a graph with multiple nodes and connections.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "addon1", Some("app1"), Some("group1")),
                create_test_node("ext2", "addon2", Some("app1"), Some("group2")),
                create_test_node("ext3", "addon3", Some("app2"), Some("group3")),
            ],
            connections: Some(vec![
                create_test_connection("ext1", Some("app1"), "cmd1", "ext2", Some("app1")),
                create_test_connection("ext2", Some("app1"), "cmd2", "ext3", Some("app2")),
                create_test_connection("ext3", Some("app2"), "cmd3", "ext1", Some("app1")),
            ]),
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test case 1: Delete a node that doesn't exist.
        let result = graph_delete_extension_node(
            &mut graph,
            "non_existent".to_string(),
            "addon1".to_string(),
            Some("app1".to_string()),
            None,
        )
        .await;
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 3);
        assert_eq!(graph.connections.as_ref().unwrap().len(), 3);

        // Test case 2: Delete ext1 - should remove node and connections to/from
        // it.
        let result = graph_delete_extension_node(
            &mut graph,
            "ext1".to_string(),
            "addon1".to_string(),
            Some("app1".to_string()),
            Some("group1".to_string()),
        )
        .await;
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 2);

        // Check connections.
        // After deleting ext1:
        // - ext1's connection is removed directly.
        // - ext3's connection had only ext1 as destination, so it's removed.
        // - ext2's connection remains since it points to ext3.
        assert!(graph.connections.is_some());
        let connections = graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        // The remaining connection should be for ext2.
        assert_eq!(connections[0].loc.extension, Some("ext2".to_string()));

        // Test case 3: Delete ext3 - should remove node and connections to/from
        // it.
        let result = graph_delete_extension_node(
            &mut graph,
            "ext3".to_string(),
            "addon3".to_string(),
            Some("app2".to_string()),
            Some("group3".to_string()),
        )
        .await;
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 1);

        // After deleting ext3:
        // - ext2's connection is empty because its only destination (ext3) is gone.
        // So all connections are removed.
        assert!(graph.connections.is_none());

        // Test case 4: Delete the last node - should have no effect on
        // connections (already None).
        let result = graph_delete_extension_node(
            &mut graph,
            "ext2".to_string(),
            "addon2".to_string(),
            Some("app1".to_string()),
            Some("group2".to_string()),
        )
        .await;
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 0);
        assert!(graph.connections.is_none());
    }

    #[tokio::test]
    async fn test_delete_extension_node_multiple_message_types() {
        // Create a graph with multiple message types in connections.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "addon1", Some("app1"), None),
                create_test_node("ext2", "addon2", Some("app1"), None),
            ],
            connections: Some(vec![]),
            exposed_messages: None,
            exposed_properties: None,
        };

        // Add a connection with multiple message types.
        let connection = GraphConnection {
            loc: GraphLoc {
                app: Some("app1".to_string()),
                extension: Some("ext1".to_string()),
                subgraph: None,
                selector: None,
            },
            cmd: Some(vec![GraphMessageFlow::new(
                Some("cmd1".to_string()),
                None,
                vec![GraphDestination {
                    loc: GraphLoc {
                        app: Some("app1".to_string()),
                        extension: Some("ext2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                vec![],
            )]),
            data: Some(vec![GraphMessageFlow::new(
                Some("data1".to_string()),
                None,
                vec![GraphDestination {
                    loc: GraphLoc {
                        app: Some("app1".to_string()),
                        extension: Some("ext2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                vec![],
            )]),
            audio_frame: Some(vec![GraphMessageFlow::new(
                Some("audio1".to_string()),
                None,
                vec![GraphDestination {
                    loc: GraphLoc {
                        app: Some("app1".to_string()),
                        extension: Some("ext2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                vec![],
            )]),
            video_frame: Some(vec![GraphMessageFlow::new(
                Some("video1".to_string()),
                None,
                vec![GraphDestination {
                    loc: GraphLoc {
                        app: Some("app1".to_string()),
                        extension: Some("ext2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: None,
                }],
                vec![],
            )]),
        };

        graph.connections.as_mut().unwrap().push(connection);

        // Delete ext2 - should remove all destinations referring to it.
        // This will result in all message flows becoming empty, and since all
        // message flows are empty, the connection will be removed, and since no
        // connections are left, connections will be set to None.
        let result = graph_delete_extension_node(
            &mut graph,
            "ext2".to_string(),
            "addon2".to_string(),
            Some("app1".to_string()),
            None,
        )
        .await;

        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 1);

        // All connections should be removed since all had only ext2 as
        // destinations.
        assert!(graph.connections.is_none());
    }
}
