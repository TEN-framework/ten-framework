//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::{collections::HashMap, path::Path};

    use ten_rust::{
        base_dir_pkg_info::PkgsInfoInApp,
        graph::{
            connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow},
            graph_info::GraphInfo,
            node::{GraphNode, GraphResource},
            Graph,
        },
        pkg_info::get_app_installed_pkgs,
    };
    use uuid::Uuid;

    #[tokio::test]
    async fn test_graph_check_extension_not_installed_1() {
        let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

        let app_dir = "tests/test_data/graph_check_extension_not_installed_1";
        let pkgs_info_in_app =
            get_app_installed_pkgs(Path::new(app_dir), true, &mut Some(&mut graphs_cache))
                .await
                .unwrap();
        assert!(!pkgs_info_in_app.is_empty());

        let (_, graph_info) = graphs_cache.into_iter().next().unwrap();

        let graph = &graph_info.graph;

        let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
        pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

        let result = graph.graph.check(&Some(app_dir.to_string()), &pkgs_cache);
        assert!(result.is_err());
        println!("Error: {:?}", result.err().unwrap());
    }

    #[tokio::test]
    async fn test_graph_check_extension_not_installed_2() {
        let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

        let app_dir = "tests/test_data/graph_check_extension_not_installed_2";
        let pkgs_info_in_app =
            get_app_installed_pkgs(Path::new(app_dir), true, &mut Some(&mut graphs_cache))
                .await
                .unwrap();
        assert!(!pkgs_info_in_app.is_empty());

        let (_, graph_info) = graphs_cache.into_iter().next().unwrap();
        let graph = &graph_info.graph;

        let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
        pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

        let result = graph.graph.check(&Some(app_dir.to_string()), &pkgs_cache);
        assert!(result.is_err());
        println!("Error: {:?}", result.err().unwrap());
    }

    #[tokio::test]
    async fn test_graph_check_predefined_graph_success() {
        let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

        let app_dir = "tests/test_data/graph_check_predefined_graph_success";
        let pkgs_info_in_app =
            get_app_installed_pkgs(Path::new(app_dir), true, &mut Some(&mut graphs_cache))
                .await
                .unwrap();
        assert!(!pkgs_info_in_app.is_empty());

        let (_, graph_info) = graphs_cache.into_iter().next().unwrap();
        let graph = &graph_info.graph;

        let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
        pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

        let result = graph.graph.check(&Some(app_dir.to_string()), &pkgs_cache);
        eprintln!("result: {result:?}");
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_graph_check_all_msgs_schema_incompatible() {
        let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

        let app_dir = "tests/test_data/graph_check_all_msgs_schema_incompatible";
        let pkgs_info_in_app =
            get_app_installed_pkgs(Path::new(app_dir), true, &mut Some(&mut graphs_cache))
                .await
                .unwrap();
        assert!(!pkgs_info_in_app.is_empty());

        let (_, graph_info) = graphs_cache.into_iter().next().unwrap();
        let graph = &graph_info.graph;

        let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
        pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

        let result = graph.graph.check(&Some(app_dir.to_string()), &pkgs_cache);
        assert!(result.is_err());
        println!("Error: {:?}", result.err().unwrap());
    }

    #[tokio::test]
    async fn test_graph_check_single_app() {
        let app_dir = "tests/test_data/graph_check_single_app";
        let pkgs_info_in_app =
            get_app_installed_pkgs(Path::new(app_dir), true, &mut Some(&mut HashMap::new()))
                .await
                .unwrap();
        assert!(!pkgs_info_in_app.is_empty());

        let graph_json_str = include_str!("../test_data/graph_check_single_app/graph.json");
        let graph = Graph::from_str_with_base_dir(graph_json_str, None).await.unwrap();

        let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
        pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

        // The schema of 'ext_c' is not found, but it's OK because we only check
        // for the app 'http://localhost:8001'.
        let result = graph.check_for_single_app(&Some(app_dir.to_string()), &pkgs_cache);
        eprintln!("result: {result:?}");
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_graph_check_builtin_extension() {
        let app_dir = "tests/test_data/graph_check_builtin_extension";
        let pkgs_info_in_app =
            get_app_installed_pkgs(Path::new(app_dir), true, &mut Some(&mut HashMap::new()))
                .await
                .unwrap();
        assert!(!pkgs_info_in_app.is_empty());

        let graph_json_str = include_str!("../test_data/graph_check_builtin_extension/graph.json");
        let graph = Graph::from_str_with_base_dir(graph_json_str, None).await.unwrap();

        let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
        pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

        let result = graph.check_for_single_app(&Some(app_dir.to_string()), &pkgs_cache);
        eprintln!("result: {result:?}");
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_graph_check_subgraph_reference_missing() {
        // Test that subgraph references in connections are validated
        let graph_json = r#"
        {
            "nodes": [
                {
                    "type": "extension",
                    "name": "ext_a",
                    "addon": "addon_a",
                    "extension_group": "some_group"
                }
            ],
            "connections": [
                {
                    "extension": "ext_a",
                    "cmd": [
                        {
                            "name": "test_cmd",
                            "dest": [
                                {
                                    "extension": "subgraph_1:ext_b"
                                }
                            ]
                        }
                    ]
                }
            ]
        }
        "#;

        let graph = Graph::from_str_with_base_dir(graph_json, None).await.unwrap();
        let pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();

        let result = graph.check(&None, &pkgs_cache);
        assert!(result.is_err());

        let error_msg = result.err().unwrap().to_string();
        assert!(error_msg.contains("subgraph 'subgraph_1'"));
        assert!(error_msg.contains("is not defined in nodes"));
    }

    #[tokio::test]
    async fn test_graph_check_subgraph_reference_valid() {
        // Test that valid subgraph references pass validation
        // Construct the graph directly to avoid triggering file loading during
        // parsing
        let graph = Graph {
            nodes: vec![
                GraphNode::new_extension_node(
                    "ext_a".to_string(),
                    "addon_a".to_string(),
                    Some("some_group".to_string()),
                    None,
                    None,
                ),
                GraphNode::new_subgraph_node(
                    "subgraph_1".to_string(),
                    None,
                    GraphResource {
                        import_uri: "/tmp/subgraph.json".to_string(),
                    },
                ),
            ],
            connections: Some(vec![GraphConnection {
                loc: GraphLoc {
                    app: None,
                    extension: Some("ext_a".to_string()),
                    subgraph: None,
                    selector: None,
                },
                cmd: Some(vec![GraphMessageFlow::new(
                    Some("test_cmd".to_string()),
                    None,
                    vec![GraphDestination {
                        loc: GraphLoc {
                            app: None,
                            extension: Some("subgraph_1:ext_b".to_string()),
                            subgraph: None,
                            selector: None,
                        },
                        msg_conversion: None,
                    }],
                    vec![],
                )]),
                data: None,
                audio_frame: None,
                video_frame: None,
            }]),
            exposed_messages: None,
            exposed_properties: None,
        };

        let pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();

        let result = graph.check(&None, &pkgs_cache);
        // This should fail due to missing extension installation, but not due
        // to subgraph reference
        assert!(result.is_err());

        let error_msg = result.err().unwrap().to_string();
        eprintln!("error_msg: {error_msg}");
        // Should not contain subgraph error
        assert!(!error_msg.contains("subgraph 'subgraph_1'"));
    }

    #[tokio::test]
    async fn test_graph_check_direct_subgraph_reference_missing() {
        // Test that direct subgraph references in connections are validated
        let graph_json = r#"
        {
            "nodes": [
                {
                    "type": "extension",
                    "name": "ext_a",
                    "addon": "addon_a",
                    "extension_group": "some_group"
                }
            ],
            "connections": [
                {
                    "subgraph": "missing_subgraph",
                    "cmd": [
                        {
                            "name": "test_cmd",
                            "dest": [
                                {
                                    "extension": "ext_a"
                                }
                            ]
                        }
                    ]
                }
            ]
        }
        "#;

        let graph = Graph::from_str_with_base_dir(graph_json, None).await.unwrap();
        let pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();

        let result = graph.check(&None, &pkgs_cache);
        assert!(result.is_err());
        let error_msg = result.err().unwrap().to_string();
        assert!(error_msg.contains("subgraph 'missing_subgraph'"));
        assert!(error_msg.contains("is not defined in nodes"));
    }
}
