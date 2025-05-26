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

    use ten_rust::graph::{graph_info::GraphInfo, node::GraphNodeType, Graph};

    #[test]
    fn test_graph_source_uri() {
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

        // Create a GraphInfo with source_uri pointing to the test graph file.
        let source_uri = graph_file_path.to_str().unwrap().to_string();
        let mut graph_info = GraphInfo {
            name: Some("test_graph".to_string()),
            auto_start: Some(true),
            graph: Graph {
                nodes: Vec::new(),
                connections: None,
                exposed_messages: None,
                exposed_properties: None,
            },
            source_uri: Some(source_uri),
            app_base_dir: None,
            belonging_pkg_type: None,
            belonging_pkg_name: None,
        };

        // Validate and complete (this should load the graph from source_uri).
        graph_info.validate_and_complete().unwrap();

        // Verify that the graph was loaded correctly.
        assert_eq!(graph_info.graph.nodes.len(), 1);
        assert_eq!(graph_info.graph.nodes[0].type_, GraphNodeType::Extension);
        assert_eq!(
            graph_info.graph.nodes[0].addon,
            Some("test_addon".to_string())
        );
        assert!(graph_info.graph.connections.is_some());
        let connections = graph_info.graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);
        assert_eq!(connections[0].loc.extension, Some("test_ext".to_string()));
    }
}
