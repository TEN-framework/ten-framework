//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::{
        connection::{
            GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow,
            GraphSource,
        },
        Graph,
    };

    #[test]
    fn test_empty_graph() {
        // Empty graph
        let empty_graph = Graph {
            nodes: vec![],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };
        assert!(Graph::convert_reversed_connections_to_forward_connections(
            &empty_graph
        )
        .unwrap()
        .is_none());
    }

    #[test]
    fn test_no_reverse_connections() {
        // Graph without reverse connections
        let mut graph_no_reverse = Graph {
            nodes: vec![],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };
        let mut conn = GraphConnection {
            loc: GraphLoc {
                app: Some("app1".to_string()),
                extension: Some("ext1".to_string()),
                subgraph: None,
            },
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        };
        conn.cmd = Some(vec![GraphMessageFlow {
            name: "flow1".to_string(),
            dest: vec![GraphDestination {
                loc: GraphLoc {
                    app: Some("app2".to_string()),
                    extension: Some("ext2".to_string()),
                    subgraph: None,
                },
                msg_conversion: None,
            }],
            source: vec![],
        }]);
        graph_no_reverse.connections = Some(vec![conn]);
        assert!(Graph::convert_reversed_connections_to_forward_connections(
            &graph_no_reverse
        )
        .unwrap()
        .is_none());
    }

    #[test]
    fn test_basic_reverse_connection() {
        // Basic reverse connection conversion
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_connection_with_source.json"
        ))
        .unwrap();

        // First verify the structure of the original graph
        assert_eq!(graph.nodes.len(), 2);
        assert_eq!(graph.connections.as_ref().unwrap().len(), 1);

        // Verify details of the original connection
        let original_conn = &graph.connections.as_ref().unwrap()[0];
        assert_eq!(
            original_conn.loc.extension,
            Some("some_extension".to_string())
        );

        let original_flow = &original_conn.cmd.as_ref().unwrap()[0];
        assert_eq!(original_flow.name, "hello");
        assert_eq!(original_flow.source.len(), 1);
        assert_eq!(
            original_flow.source[0].loc.extension,
            Some("another_ext".to_string())
        );

        // Convert to forward connections
        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph)
                .unwrap()
                .unwrap();

        println!(
            "converted: {}",
            serde_json::to_string_pretty(&converted).unwrap()
        );

        // Verify structure of the converted graph
        assert_eq!(converted.nodes.len(), 2);
        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);

        // Verify the converted connection
        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, Some("another_ext".to_string()));

        let forward_flow = &forward_conn.cmd.as_ref().unwrap()[0];
        assert_eq!(forward_flow.name, "hello");
        assert_eq!(forward_flow.source.len(), 0);
        assert_eq!(forward_flow.dest.len(), 1);
        assert_eq!(
            forward_flow.dest[0].loc.extension,
            Some("some_extension".to_string())
        );
    }

    #[test]
    fn test_multi_type_flows() {
        let mut graph_multi_types = Graph {
            nodes: vec![],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };
        let mut conn = GraphConnection {
            loc: GraphLoc {
                app: Some("app1".to_string()),
                extension: Some("ext1".to_string()),
                subgraph: None,
            },
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        };
        let flow = GraphMessageFlow {
            name: "flow1".to_string(),
            dest: vec![],
            source: vec![GraphSource {
                loc: GraphLoc {
                    app: Some("app2".to_string()),
                    extension: Some("ext2".to_string()),
                    subgraph: None,
                },
            }],
        };
        conn.cmd = Some(vec![flow.clone()]);
        conn.data = Some(vec![flow.clone()]);
        conn.audio_frame = Some(vec![flow.clone()]);
        conn.video_frame = Some(vec![flow]);
        graph_multi_types.connections = Some(vec![conn]);
        let converted =
            Graph::convert_reversed_connections_to_forward_connections(
                &graph_multi_types,
            )
            .unwrap()
            .unwrap();
        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);
        assert_eq!(
            converted.connections.as_ref().unwrap()[0]
                .cmd
                .as_ref()
                .unwrap()
                .len(),
            1
        );

        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, Some("ext2".to_string()));
        assert_eq!(forward_conn.loc.app, Some("app2".to_string()));

        let forward_flow =
            &converted.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap()
                [0];
        assert_eq!(forward_flow.name, "flow1");
        assert_eq!(forward_flow.source.len(), 0);
        assert_eq!(forward_flow.dest.len(), 1);
        assert_eq!(
            forward_flow.dest[0].loc.extension,
            Some("ext1".to_string())
        );
        assert_eq!(forward_flow.dest[0].loc.app, Some("app1".to_string()));
        assert_eq!(forward_flow.dest[0].loc.subgraph, None);
    }

    #[test]
    fn test_merge_duplicate_connections() {
        // Basic reverse connection conversion
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_connection_duplicate_with_source.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph)
                .unwrap()
                .unwrap();

        println!(
            "converted: {}",
            serde_json::to_string_pretty(&converted).unwrap()
        );

        // The converted graph should have 2 nodes and 1 connection.
        // The original reverse connection should be merged into one forward
        // connection.
        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);
        assert_eq!(
            converted.connections.as_ref().unwrap()[0].loc.extension,
            Some("another_ext".to_string())
        );
        assert_eq!(
            converted.connections.as_ref().unwrap()[0]
                .cmd
                .as_ref()
                .unwrap()
                .len(),
            1
        );
        assert_eq!(
            converted.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap()[0]
                .dest[0]
                .loc
                .extension,
            Some("some_extension".to_string())
        );
    }

    #[test]
    fn test_empty_source_handling() {
        // 测试用例6：空source的消息流处理
        let mut graph_empty_source = Graph {
            nodes: vec![],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };
        let mut conn = GraphConnection {
            loc: GraphLoc {
                app: Some("app1".to_string()),
                extension: Some("ext1".to_string()),
                subgraph: None,
            },
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        };
        conn.cmd = Some(vec![
            GraphMessageFlow {
                name: "flow1".to_string(),
                dest: vec![],
                source: vec![],
            },
            GraphMessageFlow {
                name: "flow2".to_string(),
                dest: vec![],
                source: vec![GraphSource {
                    loc: GraphLoc {
                        app: Some("app2".to_string()),
                        extension: Some("ext2".to_string()),
                        subgraph: None,
                    },
                }],
            },
        ]);
        graph_empty_source.connections = Some(vec![conn]);
        let converted =
            Graph::convert_reversed_connections_to_forward_connections(
                &graph_empty_source,
            )
            .unwrap()
            .unwrap();
        assert_eq!(converted.connections.as_ref().unwrap().len(), 2);
    }

    #[test]
    fn test_complex_scenario() {
        // 测试用例7：复杂场景 - 多个source和destination
        let mut graph_complex = Graph {
            nodes: vec![],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };
        let mut conn = GraphConnection {
            loc: GraphLoc {
                app: Some("app1".to_string()),
                extension: Some("ext1".to_string()),
                subgraph: None,
            },
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        };
        conn.cmd = Some(vec![GraphMessageFlow {
            name: "flow1".to_string(),
            dest: vec![
                GraphDestination {
                    loc: GraphLoc {
                        app: Some("dest1".to_string()),
                        extension: Some("ext1".to_string()),
                        subgraph: None,
                    },
                    msg_conversion: None,
                },
                GraphDestination {
                    loc: GraphLoc {
                        app: Some("dest2".to_string()),
                        extension: Some("ext2".to_string()),
                        subgraph: None,
                    },
                    msg_conversion: None,
                },
            ],
            source: vec![
                GraphSource {
                    loc: GraphLoc {
                        app: Some("source1".to_string()),
                        extension: Some("ext1".to_string()),
                        subgraph: None,
                    },
                },
                GraphSource {
                    loc: GraphLoc {
                        app: Some("source2".to_string()),
                        extension: Some("ext2".to_string()),
                        subgraph: None,
                    },
                },
            ],
        }]);
        graph_complex.connections = Some(vec![conn]);
        let converted =
            Graph::convert_reversed_connections_to_forward_connections(
                &graph_complex,
            )
            .unwrap()
            .unwrap();
        assert_eq!(converted.connections.as_ref().unwrap().len(), 3);
    }
}
