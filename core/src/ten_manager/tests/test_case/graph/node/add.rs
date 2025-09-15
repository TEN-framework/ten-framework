//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_manager::graph::nodes::add::graph_add_extension_node;
    use ten_rust::{graph::Graph, pkg_info::localhost};

    #[tokio::test]
    async fn test_add_extension_node() {
        // Create an empty graph.
        let mut graph = Graph {
            nodes: Vec::new(),
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test case 1: Add a valid node.
        let result = graph_add_extension_node(
            &mut graph,
            "test_extension",
            "test_addon",
            &Some("http://test-app-uri.com".to_string()),
            &None,
            &None,
        )
        .await;
        eprintln!("result: {result:?}");
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 1);
        if let ten_rust::graph::node::GraphNode::Extension {
            content,
        } = &graph.nodes[0]
        {
            assert_eq!(content.name, "test_extension");
            assert_eq!(content.addon, "test_addon");
            assert_eq!(content.app, Some("http://test-app-uri.com".to_string()));
        } else {
            panic!("Expected Extension node");
        }

        // Test case 2: Add a second node.
        let result = graph_add_extension_node(
            &mut graph,
            "test_extension2",
            "test_addon2",
            &Some("http://test-app-uri.com".to_string()), // Same app URI.
            &Some("custom_group".to_string()),
            &None,
        )
        .await;
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 2);
        if let ten_rust::graph::node::GraphNode::Extension {
            content,
        } = &graph.nodes[1]
        {
            assert_eq!(content.name, "test_extension2");
            assert_eq!(content.extension_group, Some("custom_group".to_string()));
        } else {
            panic!("Expected Extension node");
        }

        // Test case 3: Adding a node with localhost app URI should fail.
        let original_len = graph.nodes.len();
        let result = graph_add_extension_node(
            &mut graph,
            "test_extension3",
            "test_addon3",
            &Some(localhost().to_string()), // This is not allowed.
            &None,
            &None,
        )
        .await;
        assert!(result.is_err());
        // Verify rollback worked.
        assert_eq!(graph.nodes.len(), original_len);

        // Test case 4: Adding a node with different app URI.
        let original_len = graph.nodes.len();
        let result = graph_add_extension_node(
            &mut graph,
            "test_extension4",
            "test_addon4",
            &Some("http://different-uri.com".to_string()), // Different URI.
            &None,
            &None,
        )
        .await;
        // This should be ok as mixed URIs are valid.
        assert!(result.is_ok());
        // Node should be added.
        assert_eq!(graph.nodes.len(), original_len + 1);

        // Test case 5: Adding a node with the same extension and app URI.
        let original_len = graph.nodes.len();
        let result = graph_add_extension_node(
            &mut graph,
            "test_extension4",
            "test_addon4",
            &Some("http://different-uri.com".to_string()),
            &None,
            &None,
        )
        .await;
        // This should fail because the node already exists.
        assert!(result.is_err());
        assert_eq!(graph.nodes.len(), original_len);

        // Test case 6: Adding a node with no app URI to a graph with declared
        // app URIs. This should fail because we would have mixed app
        // declarations (some with URI, some without).
        let original_len = graph.nodes.len();
        let result = graph_add_extension_node(
            &mut graph,
            "test_extension5",
            "test_addon5",
            &None, // No app URI.
            &None,
            &None,
        )
        .await;
        assert!(result.is_err());
        // Verify rollback worked.
        assert_eq!(graph.nodes.len(), original_len);
    }
}
