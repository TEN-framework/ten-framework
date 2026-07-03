//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::{
        connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow, GraphSource},
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
        assert!(Graph::convert_reversed_connections_to_forward_connections(&empty_graph)
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
                selector: None,
            },
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        };
        conn.cmd = Some(vec![GraphMessageFlow {
            name: Some("flow1".to_string()),
            names: None,
            dest: vec![GraphDestination {
                loc: GraphLoc {
                    app: Some("app2".to_string()),
                    extension: Some("ext2".to_string()),
                    subgraph: None,
                    selector: None,
                },
                msg_conversion: None,
            }],
            source: vec![],
        }]);
        graph_no_reverse.connections = Some(vec![conn]);
        assert!(Graph::convert_reversed_connections_to_forward_connections(&graph_no_reverse)
            .unwrap()
            .is_none());
    }

    #[test]
    fn test_basic_reverse_connection() {
        // Basic reverse connection conversion
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_connection_with_source.json"
        ))
        .unwrap();

        // First verify the structure of the original graph
        assert_eq!(graph.nodes.len(), 2);
        assert_eq!(graph.connections.as_ref().unwrap().len(), 1);

        // Verify details of the original connection
        let original_conn = &graph.connections.as_ref().unwrap()[0];
        assert_eq!(original_conn.loc.extension, Some("some_extension".to_string()));

        let original_flow = &original_conn.cmd.as_ref().unwrap()[0];
        assert_eq!(original_flow.name.as_deref(), Some("hello"));
        assert_eq!(original_flow.source.len(), 1);
        assert_eq!(original_flow.source[0].loc.extension, Some("another_ext".to_string()));

        // Convert to forward connections
        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!("converted: {}", serde_json::to_string_pretty(&converted).unwrap());

        // Verify structure of the converted graph
        assert_eq!(converted.nodes.len(), 2);
        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);

        // Verify the converted connection
        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, Some("another_ext".to_string()));

        let forward_flow = &forward_conn.cmd.as_ref().unwrap()[0];
        assert_eq!(forward_flow.name.as_deref(), Some("hello"));
        assert_eq!(forward_flow.source.len(), 0);
        assert_eq!(forward_flow.dest.len(), 1);
        assert_eq!(forward_flow.dest[0].loc.extension, Some("some_extension".to_string()));
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
                selector: None,
            },
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        };
        let flow = GraphMessageFlow {
            name: Some("flow1".to_string()),
            names: None,
            dest: vec![],
            source: vec![GraphSource {
                loc: GraphLoc {
                    app: Some("app2".to_string()),
                    extension: Some("ext2".to_string()),
                    subgraph: None,
                    selector: None,
                },
            }],
        };
        conn.cmd = Some(vec![flow.clone()]);
        conn.data = Some(vec![flow.clone()]);
        conn.audio_frame = Some(vec![flow.clone()]);
        conn.video_frame = Some(vec![flow]);
        graph_multi_types.connections = Some(vec![conn]);
        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph_multi_types)
                .unwrap()
                .unwrap();
        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);
        assert_eq!(converted.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap().len(), 1);

        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, Some("ext2".to_string()));
        assert_eq!(forward_conn.loc.app, Some("app2".to_string()));

        let forward_flow = &converted.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap()[0];
        assert_eq!(forward_flow.name.as_deref(), Some("flow1"));
        assert_eq!(forward_flow.source.len(), 0);
        assert_eq!(forward_flow.dest.len(), 1);
        assert_eq!(forward_flow.dest[0].loc.extension, Some("ext1".to_string()));
        assert_eq!(forward_flow.dest[0].loc.app, Some("app1".to_string()));
        assert_eq!(forward_flow.dest[0].loc.subgraph, None);
    }

    #[test]
    fn test_merge_duplicate_connections() {
        // Basic reverse connection conversion
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_connection_duplicate_with_source.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!("converted: {}", serde_json::to_string_pretty(&converted).unwrap());

        // The converted graph should have 2 nodes and 1 connection.
        // The original reverse connection should be merged into one forward
        // connection.
        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);
        assert_eq!(
            converted.connections.as_ref().unwrap()[0].loc.extension,
            Some("another_ext".to_string())
        );
        assert_eq!(converted.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap().len(), 1);
        assert_eq!(
            converted.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap()[0].dest[0]
                .loc
                .extension,
            Some("some_extension".to_string())
        );
    }

    #[test]
    fn test_multiple_sources_reverse_connection() {
        // Test reverse connection with multiple sources
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_multiple_sources.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!(
            "converted multiple sources: {}",
            serde_json::to_string_pretty(&converted).unwrap()
        );

        // Should create 2 forward connections, one for each source
        assert_eq!(converted.connections.as_ref().unwrap().len(), 2);

        // Verify both forward connections exist
        let connections = converted.connections.as_ref().unwrap();
        let source_1_conn = connections
            .iter()
            .find(|c| c.loc.extension == Some("source_ext_1".to_string()))
            .expect("source_ext_1 connection should exist");
        let source_2_conn = connections
            .iter()
            .find(|c| c.loc.extension == Some("source_ext_2".to_string()))
            .expect("source_ext_2 connection should exist");

        // Verify both connections have the same destination
        assert_eq!(
            source_1_conn.cmd.as_ref().unwrap()[0].dest[0].loc.extension,
            Some("destination_ext".to_string())
        );
        assert_eq!(
            source_2_conn.cmd.as_ref().unwrap()[0].dest[0].loc.extension,
            Some("destination_ext".to_string())
        );
    }

    #[test]
    fn test_mixed_forward_reverse_connections() {
        // Test graph with both forward and reverse connections
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_mixed_forward_reverse.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!("converted mixed: {}", serde_json::to_string_pretty(&converted).unwrap());

        assert_eq!(converted.connections.as_ref().unwrap().len(), 2);

        // Should have original forward connection (with source cleared) and new
        // reverse connection
        let ext_a_conn = converted
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("ext_a".to_string()))
            .expect("ext_a connection should exist");
        let ext_c_conn = converted
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("ext_c".to_string()))
            .expect("ext_c connection should exist");

        // Verify forward connection is preserved
        let forward_flow = ext_a_conn
            .cmd
            .as_ref()
            .unwrap()
            .iter()
            .find(|f| f.name.as_deref() == Some("forward_cmd"))
            .expect("forward_cmd should exist");
        assert_eq!(forward_flow.dest[0].loc.extension, Some("ext_b".to_string()));

        // Verify reverse connection is converted
        let reverse_flow = ext_c_conn
            .cmd
            .as_ref()
            .unwrap()
            .iter()
            .find(|f| f.name.as_deref() == Some("reverse_cmd"))
            .expect("reverse_cmd should exist");
        assert_eq!(reverse_flow.dest[0].loc.extension, Some("ext_a".to_string()));
    }

    #[test]
    fn test_app_and_subgraph_fields() {
        // Test reverse connections with app and subgraph fields
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_with_app_and_subgraph.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!(
            "converted app and subgraph: {}",
            serde_json::to_string_pretty(&converted).unwrap()
        );

        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);

        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, None);
        assert_eq!(forward_conn.loc.subgraph, Some("subgraph1".to_string()));

        let forward_flow = &forward_conn.cmd.as_ref().unwrap()[0];
        assert_eq!(forward_flow.name.as_deref(), Some("subgraph_call"));
        assert_eq!(forward_flow.dest[0].loc.subgraph, None);
        assert_eq!(forward_flow.dest[0].loc.extension, Some("ext1".to_string()));
    }

    #[test]
    fn test_all_message_types_reverse() {
        // Test all message types (cmd, data, audio_frame, video_frame) with
        // reverse connections
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_all_message_types_reverse.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!("converted all types: {}", serde_json::to_string_pretty(&converted).unwrap());

        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);

        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, Some("src_ext".to_string()));

        // Verify all message types are converted
        assert!(forward_conn.cmd.is_some());
        assert!(forward_conn.data.is_some());
        assert!(forward_conn.audio_frame.is_some());
        assert!(forward_conn.video_frame.is_some());

        // Verify each message type has correct destination
        assert_eq!(
            forward_conn.cmd.as_ref().unwrap()[0].dest[0].loc.extension,
            Some("dest_ext".to_string())
        );
        assert_eq!(
            forward_conn.data.as_ref().unwrap()[0].dest[0].loc.extension,
            Some("dest_ext".to_string())
        );
        assert_eq!(
            forward_conn.audio_frame.as_ref().unwrap()[0].dest[0].loc.extension,
            Some("dest_ext".to_string())
        );
        assert_eq!(
            forward_conn.video_frame.as_ref().unwrap()[0].dest[0].loc.extension,
            Some("dest_ext".to_string())
        );
    }

    #[test]
    fn test_multiple_flows_same_connection() {
        // Test connection with multiple flows, some forward, some reverse
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_multiple_flows_same_connection.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!("converted multiple flows: {}", serde_json::to_string_pretty(&converted).unwrap());

        assert_eq!(converted.connections.as_ref().unwrap().len(), 2);

        // Should have the original connection (with reverse flows removed) and
        // a new forward connection
        let dest_conn = converted
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("multi_flow_dest".to_string()))
            .expect("multi_flow_dest connection should exist");
        let src_conn = converted
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("flow_source".to_string()))
            .expect("flow_source connection should exist");

        // Original connection should only have the forward flow
        assert_eq!(dest_conn.cmd.as_ref().unwrap().len(), 1);
        assert_eq!(dest_conn.cmd.as_ref().unwrap()[0].name.as_deref(), Some("flow_gamma"));

        // New forward connection should have 2 flows (from the 2 reverse flows)
        assert_eq!(src_conn.cmd.as_ref().unwrap().len(), 2);
        let flow_names: Vec<&str> =
            src_conn.cmd.as_ref().unwrap().iter().map(|f| f.name.as_deref().unwrap()).collect();
        assert!(flow_names.contains(&"flow_alpha"));
        assert!(flow_names.contains(&"flow_beta"));
    }

    #[test]
    fn test_source_only_flows() {
        // Test flows that have only sources without destinations
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_source_only_flows.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!("converted source only: {}", serde_json::to_string_pretty(&converted).unwrap());

        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);

        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, Some("source_ext".to_string()));

        let forward_flow = &forward_conn.cmd.as_ref().unwrap()[0];
        assert_eq!(forward_flow.name.as_deref(), Some("source_only_flow"));
        assert_eq!(forward_flow.dest[0].loc.extension, Some("empty_dest_ext".to_string()));
    }

    #[test]
    fn test_empty_source_arrays() {
        // Test connections with empty source arrays (should return None)
        let mut graph_empty_sources = Graph {
            nodes: vec![],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };
        let mut conn = GraphConnection {
            loc: GraphLoc {
                app: None,
                extension: Some("ext1".to_string()),
                subgraph: None,
                selector: None,
            },
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        };
        conn.cmd = Some(vec![GraphMessageFlow {
            name: Some("empty_source_flow".to_string()),
            names: None,
            dest: vec![],
            source: vec![], // Empty source array
        }]);
        graph_empty_sources.connections = Some(vec![conn]);

        let result =
            Graph::convert_reversed_connections_to_forward_connections(&graph_empty_sources)
                .unwrap();
        assert!(result.is_none());
    }

    #[test]
    fn test_complex_merge_scenario() {
        // Test complex scenario with multiple connections that need merging
        let graph_complex: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_complex_merge_scenario.json"
        ))
        .unwrap();

        // First verify the structure of the original graph
        assert_eq!(graph_complex.nodes.len(), 3);
        assert_eq!(graph_complex.connections.as_ref().unwrap().len(), 2);

        // Verify details of original connections
        let original_connections = graph_complex.connections.as_ref().unwrap();
        assert!(original_connections.iter().any(|c| c.loc.extension == Some("ext1".to_string())
            && c.cmd.as_ref().unwrap()[0].name.as_deref() == Some("shared_flow")
            && c.cmd.as_ref().unwrap()[0].source[0].loc.extension == Some("ext2".to_string())));
        assert!(original_connections.iter().any(|c| c.loc.extension == Some("ext3".to_string())
            && c.cmd.as_ref().unwrap()[0].name.as_deref() == Some("shared_flow")
            && c.cmd.as_ref().unwrap()[0].source[0].loc.extension == Some("ext2".to_string())));

        // Convert to forward connections
        let converted = Graph::convert_reversed_connections_to_forward_connections(&graph_complex)
            .unwrap()
            .unwrap();

        println!("converted complex merge: {}", serde_json::to_string_pretty(&converted).unwrap());

        // Verify structure of the converted graph
        assert_eq!(converted.nodes.len(), 3);
        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);

        // Verify the converted connection
        let forward_conn = &converted.connections.as_ref().unwrap()[0];
        assert_eq!(forward_conn.loc.extension, Some("ext2".to_string()));

        let forward_flow = &forward_conn.cmd.as_ref().unwrap()[0];
        assert_eq!(forward_flow.name.as_deref(), Some("shared_flow"));
        assert_eq!(forward_flow.source.len(), 0);
        assert_eq!(forward_flow.dest.len(), 2);

        // Verify both destinations are present
        let dest_extensions: Vec<&str> =
            forward_flow.dest.iter().map(|d| d.loc.extension.as_ref().unwrap().as_str()).collect();
        assert!(dest_extensions.contains(&"ext1"));
        assert!(dest_extensions.contains(&"ext3"));
    }

    #[test]
    fn test_graph_with_app_uri() {
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_sources/graph_with_app_uri.json"
        ))
        .unwrap();

        let converted =
            Graph::convert_reversed_connections_to_forward_connections(&graph).unwrap().unwrap();

        println!(
            "converted graph with app uri: {}",
            serde_json::to_string_pretty(&converted).unwrap()
        );

        assert_eq!(converted.connections.as_ref().unwrap().len(), 1);
        assert_eq!(
            converted.connections.as_ref().unwrap()[0].loc.app,
            Some("msgpack://127.0.0.1:8001/".to_string())
        );
        assert_eq!(
            converted.connections.as_ref().unwrap()[0].loc.extension,
            Some("test_extension_1".to_string())
        );
        assert_eq!(
            converted.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap()[0].dest[0]
                .loc
                .extension,
            Some("test_extension_2".to_string())
        );
    }
}
