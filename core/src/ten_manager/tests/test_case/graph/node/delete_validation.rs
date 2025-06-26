//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_manager::designer::graphs::nodes::delete::graph_delete_extension_node;
    use ten_rust::{
        graph::{
            connection::{
                GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow,
            },
            node::{GraphNode, GraphNodeType},
            Graph,
        },
        pkg_info::localhost,
    };

    #[tokio::test]
    async fn test_delete_extension_node_with_validation_success() {
        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "test_extension_1".to_string(),
                    addon: Some("test_addon_1".to_string()),
                    extension_group: None,
                    app: Some("http://test-app-uri.com".to_string()),
                    property: None,
                    import_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "test_extension_2".to_string(),
                    addon: Some("test_addon_2".to_string()),
                    extension_group: None,
                    app: Some("http://test-app-uri.com".to_string()),
                    property: None,
                    import_uri: None,
                },
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Test case 1: Delete a valid node - should succeed.
        let result = graph_delete_extension_node(
            &mut graph,
            "test_extension_1".to_string(),
            "test_addon_1".to_string(),
            Some("http://test-app-uri.com".to_string()),
            None,
        )
        .await;
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 1);
        assert_eq!(graph.nodes[0].name, "test_extension_2");

        // Test case 2: Delete the remaining node - should also succeed
        // because validate_and_complete_and_flatten() doesn't check for minimum
        // extension count.
        let result = graph_delete_extension_node(
            &mut graph,
            "test_extension_2".to_string(),
            "test_addon_2".to_string(),
            Some("http://test-app-uri.com".to_string()),
            None,
        )
        .await;
        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 0);
    }

    #[tokio::test]
    async fn test_delete_extension_node_validation_failure_restores_graph() {
        // Create a custom delete function that will cause validation to fail
        // by modifying the remaining node to have an invalid state.
        async fn graph_delete_extension_node_with_corruption(
            graph: &mut Graph,
            pkg_name: String,
            addon: String,
            app: Option<String>,
            extension_group: Option<String>,
        ) -> Result<(), anyhow::Error> {
            // Store the original state in case validation fails.
            let original_graph = graph.clone();

            // Find and remove the matching node.
            let original_nodes_len = graph.nodes.len();
            graph.nodes.retain(|node| {
                !(node.type_ == GraphNodeType::Extension
                    && node.name == pkg_name
                    && node.addon == Some(addon.clone())
                    && node.app == app
                    && node.extension_group == extension_group)
            });

            // If no node was removed, return early.
            if graph.nodes.len() == original_nodes_len {
                return Ok(());
            }

            // Corrupt the remaining node to cause validation failure
            if !graph.nodes.is_empty() {
                graph.nodes[0].app = Some(localhost().to_string()); // This will
                                                                    // cause validation
                                                                    // to fail
            }

            // Validate the graph.
            match graph.validate_and_complete_and_flatten(None).await {
                Ok(_) => Ok(()),
                Err(e) => {
                    // Restore the original graph if validation fails.
                    *graph = original_graph;
                    Err(e)
                }
            }
        }

        let mut graph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "test_extension_1".to_string(),
                    addon: Some("test_addon_1".to_string()),
                    extension_group: None,
                    app: Some("http://test-app-uri.com".to_string()),
                    property: None,
                    import_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "test_extension_2".to_string(),
                    addon: Some("test_addon_2".to_string()),
                    extension_group: None,
                    app: Some("http://test-app-uri.com".to_string()),
                    property: None,
                    import_uri: None,
                },
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        // Store the original state.
        let original_nodes_len = graph.nodes.len();

        // Try to delete test_extension_1. The custom function will corrupt the
        // remaining node, causing validation to fail.
        let result = graph_delete_extension_node_with_corruption(
            &mut graph,
            "test_extension_1".to_string(),
            "test_addon_1".to_string(),
            Some("http://test-app-uri.com".to_string()),
            None,
        )
        .await;

        // The operation should fail due to validation.
        assert!(result.is_err());

        // The graph should be restored to its original state.
        assert_eq!(graph.nodes.len(), original_nodes_len);
        assert_eq!(graph.nodes[0].name, "test_extension_1");
        assert_eq!(graph.nodes[1].name, "test_extension_2");
        // Verify the graph was properly restored (no localhost corruption)
        assert_eq!(
            graph.nodes[0].app,
            Some("http://test-app-uri.com".to_string())
        );
        assert_eq!(
            graph.nodes[1].app,
            Some("http://test-app-uri.com".to_string())
        );
    }

    #[tokio::test]
    async fn test_delete_extension_node_with_connections_cleanup() {
        // Create a graph with connections that should be cleaned up.
        let mut graph = Graph {
            nodes: vec![
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "source_ext".to_string(),
                    addon: Some("source_addon".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    import_uri: None,
                },
                GraphNode {
                    type_: GraphNodeType::Extension,
                    name: "target_ext".to_string(),
                    addon: Some("target_addon".to_string()),
                    extension_group: None,
                    app: None,
                    property: None,
                    import_uri: None,
                },
            ],
            connections: Some(vec![GraphConnection {
                loc: GraphLoc {
                    app: None,
                    extension: Some("source_ext".to_string()),
                    subgraph: None,
                },
                cmd: Some(vec![GraphMessageFlow {
                    name: "test_cmd".to_string(),
                    dest: vec![GraphDestination {
                        loc: GraphLoc {
                            app: None,
                            extension: Some("target_ext".to_string()),
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

        // Delete the target extension.
        let result = graph_delete_extension_node(
            &mut graph,
            "target_ext".to_string(),
            "target_addon".to_string(),
            None,
            None,
        )
        .await;

        assert!(result.is_ok());
        assert_eq!(graph.nodes.len(), 1);
        assert_eq!(graph.nodes[0].name, "source_ext");

        // The connections should be cleaned up since the target was removed.
        assert!(
            graph.connections.is_none()
                || graph.connections.as_ref().unwrap().is_empty()
        );
    }
}
