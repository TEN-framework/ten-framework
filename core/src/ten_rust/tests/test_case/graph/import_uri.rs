//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::fs;

    use tempfile::tempdir;
    use ten_rust::graph::{
        graph_info::{GraphContent, GraphInfo},
        node::GraphNode,
        Graph,
    };

    #[tokio::test]
    async fn test_graph_import_uri() {
        // Create a temporary graph file.
        let temp_dir = tempdir().unwrap();
        let graph_file_path = temp_dir.path().join("test_graph.json");

        // Define a test graph.
        let test_graph_str = r#"
        {
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_ext",
                    "addon": "test_addon",
                    "extension_group": "test_group"
                }
            ],
            "connections": [
                {
                    "extension": "test_ext",
                    "cmd": [
                        {
                            "name": "test_cmd",
                            "dest": [
                                {
                                    "extension": "test_ext"
                                }
                            ]
                        }
                    ]
                }
            ]
        }
        "#;

        // Write the test graph to the file.
        fs::write(&graph_file_path, test_graph_str).unwrap();

        // Create a GraphInfo with import_uri pointing to the test graph file
        // using file:// URI.
        let import_uri = format!("file://{}", graph_file_path.display());
        let mut graph_info = GraphInfo {
            name: Some("test_graph".to_string()),
            auto_start: Some(true),
            singleton: None,
            graph: GraphContent {
                import_uri: Some(import_uri),
                graph: Graph {
                    nodes: Vec::new(),
                    connections: None,
                    exposed_messages: None,
                    exposed_properties: None,
                },
                flattened_graph: None,
            },
            app_base_dir: None,
            belonging_pkg_type: None,
            belonging_pkg_name: None,
        };

        // Validate and complete (this should load the graph from import_uri).
        graph_info.validate_and_complete_and_flatten().await.unwrap();

        // Verify that the graph was loaded correctly.
        assert_eq!(graph_info.graph.nodes().len(), 1);

        if let GraphNode::Extension {
            content,
        } = &graph_info.graph.nodes()[0]
        {
            assert_eq!(content.addon, "test_addon");
        } else {
            panic!("Unexpected non-extension node in graph");
        }

        assert!(graph_info.graph.connections().is_some());
        let connections = graph_info.graph.connections().as_ref().unwrap();
        assert_eq!(connections.len(), 1);
        assert_eq!(connections[0].loc.extension, Some("test_ext".to_string()));
    }

    #[tokio::test]
    async fn test_import_uri_mutual_exclusion_with_nodes() {
        // Create a GraphInfo with both import_uri and nodes - this should fail
        let mut graph_info = GraphInfo {
            name: Some("test_graph".to_string()),
            auto_start: Some(true),
            singleton: None,
            graph: GraphContent {
                import_uri: Some("test_uri".to_string()),
                graph: Graph {
                    nodes: vec![GraphNode::new_extension_node(
                        "test_ext".to_string(),
                        "test_addon".to_string(),
                        Some("test_group".to_string()),
                        None,
                        None,
                    )],
                    connections: None,
                    exposed_messages: None,
                    exposed_properties: None,
                },
                flattened_graph: None,
            },
            app_base_dir: None,
            belonging_pkg_type: None,
            belonging_pkg_name: None,
        };

        // This should fail due to mutual exclusion
        let result = graph_info.validate_and_complete_and_flatten().await;
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("When 'import_uri' is specified, 'nodes' field must not be present"));
    }

    #[tokio::test]
    async fn test_import_uri_mutual_exclusion_with_connections() {
        use ten_rust::graph::connection::{GraphConnection, GraphLoc, GraphMessageFlow};

        // Create a GraphInfo with both import_uri and connections - this should
        // fail
        let mut graph_info = GraphInfo {
            name: Some("test_graph".to_string()),
            auto_start: Some(true),
            singleton: None,
            graph: GraphContent {
                import_uri: Some("test_uri".to_string()),
                graph: Graph {
                    nodes: Vec::new(),
                    connections: Some(vec![GraphConnection {
                        loc: GraphLoc {
                            app: None,
                            extension: Some("test_ext".to_string()),
                            subgraph: None,
                            selector: None,
                        },
                        cmd: Some(vec![GraphMessageFlow::new(
                            Some("test_cmd".to_string()),
                            None,
                            vec![],
                            vec![],
                        )]),
                        data: None,
                        audio_frame: None,
                        video_frame: None,
                    }]),
                    exposed_messages: None,
                    exposed_properties: None,
                },
                flattened_graph: None,
            },
            app_base_dir: None,
            belonging_pkg_type: None,
            belonging_pkg_name: None,
        };

        // This should fail due to mutual exclusion
        let result = graph_info.validate_and_complete_and_flatten().await;
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("When 'import_uri' is specified, 'connections' field must not be present"));
    }

    #[tokio::test]
    async fn test_import_uri_mutual_exclusion_with_exposed_messages() {
        use ten_rust::graph::{GraphExposedMessage, GraphExposedMessageType};

        // Create a GraphInfo with both import_uri and exposed_messages - this
        // should fail
        let mut graph_info = GraphInfo {
            name: Some("test_graph".to_string()),
            auto_start: Some(true),
            singleton: None,
            graph: GraphContent {
                import_uri: Some("test_uri".to_string()),
                graph: Graph {
                    nodes: Vec::new(),
                    connections: None,
                    exposed_messages: Some(vec![GraphExposedMessage {
                        msg_type: GraphExposedMessageType::CmdIn,
                        name: "test_msg".to_string(),
                        extension: Some("test_ext".to_string()),
                        subgraph: None,
                    }]),
                    exposed_properties: None,
                },
                flattened_graph: None,
            },
            app_base_dir: None,
            belonging_pkg_type: None,
            belonging_pkg_name: None,
        };

        // This should fail due to mutual exclusion
        let result = graph_info.validate_and_complete_and_flatten().await;
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains(
            "When 'import_uri' is specified, 'exposed_messages' field must not be present"
        ));
    }

    #[tokio::test]
    async fn test_import_uri_mutual_exclusion_with_exposed_properties() {
        use ten_rust::graph::GraphExposedProperty;

        // Create a GraphInfo with both import_uri and exposed_properties - this
        // should fail
        let mut graph_info = GraphInfo {
            name: Some("test_graph".to_string()),
            auto_start: Some(true),
            singleton: None,
            graph: GraphContent {
                import_uri: Some("test_uri".to_string()),
                graph: Graph {
                    nodes: Vec::new(),
                    connections: None,
                    exposed_messages: None,
                    exposed_properties: Some(vec![GraphExposedProperty {
                        extension: Some("test_ext".to_string()),
                        subgraph: None,
                        name: "test_prop".to_string(),
                    }]),
                },
                flattened_graph: None,
            },
            app_base_dir: None,
            belonging_pkg_type: None,
            belonging_pkg_name: None,
        };

        // This should fail due to mutual exclusion
        let result = graph_info.validate_and_complete_and_flatten().await;
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains(
            "When 'import_uri' is specified, 'exposed_properties' field must not be present"
        ));
    }

    #[tokio::test]
    async fn test_import_uri_without_conflicting_fields_succeeds() {
        // Create a temporary graph file.
        let temp_dir = tempdir().unwrap();
        let graph_file_path = temp_dir.path().join("test_graph.json");

        // Define a test graph.
        let test_graph_str = r#"
        {
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_ext",
                    "addon": "test_addon",
                    "extension_group": "test_group"
                }
            ]
        }
        "#;

        // Write the test graph to the file.
        fs::write(&graph_file_path, test_graph_str).unwrap();

        // Create a GraphInfo with only import_uri and empty graph fields - this
        // should succeed
        let import_uri = format!("file://{}", graph_file_path.display());
        let mut graph_info = GraphInfo {
            name: Some("test_graph".to_string()),
            auto_start: Some(true),
            singleton: None,
            graph: GraphContent {
                import_uri: Some(import_uri),
                graph: Graph {
                    nodes: Vec::new(),
                    connections: None,
                    exposed_messages: None,
                    exposed_properties: None,
                },
                flattened_graph: None,
            },
            app_base_dir: None,
            belonging_pkg_type: None,
            belonging_pkg_name: None,
        };

        // This should succeed
        let result = graph_info.validate_and_complete_and_flatten().await;
        assert!(result.is_ok());

        // Verify that the graph was loaded from import_uri
        assert_eq!(graph_info.graph.nodes().len(), 1);
        assert_eq!(graph_info.graph.nodes()[0].get_name(), "test_ext");
    }
}
