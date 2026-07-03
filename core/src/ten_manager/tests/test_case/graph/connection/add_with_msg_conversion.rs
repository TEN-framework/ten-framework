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
        graph::{
            connection::GraphLoc,
            msg_conversion::{
                MsgAndResultConversion, MsgConversion, MsgConversionMode, MsgConversionRule,
                MsgConversionRules, MsgConversionType,
            },
            node::GraphNodeType,
            Graph,
        },
        pkg_info::message::MsgType,
    };

    use crate::test_case::{
        common::mock::inject_all_standard_pkgs_for_mock, graph::connection::create_test_node,
    };

    #[tokio::test]
    async fn test_add_connection_with_fixed_value_msg_conversion() {
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

        // Create a message conversion with fixed value.
        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![
                        MsgConversionRule {
                            path: "ten.name".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::Value::String("test_value".to_string())),
                        },
                        MsgConversionRule {
                            path: "param1".to_string(),
                            conversion_mode: MsgConversionMode::FixedValue,
                            original_path: None,
                            value: Some(serde_json::Value::Number(serde_json::Number::from(42))),
                        },
                    ],
                    keep_original: Some(true),
                },
            }),
            result: None,
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
        // Test adding a connection with msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            Some(msg_conversion.clone()),
        )
        .await;

        assert!(result.is_ok());
        assert!(graph.connections.is_some());

        // Verify connection and msg_conversion were added correctly
        let connections = graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let connection = &connections[0];
        assert_eq!(connection.loc.app, Some("http://example.com:8000".to_string()));
        assert_eq!(connection.loc.extension, Some("ext1".to_string()));

        let cmd_flows = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 1);

        let flow = &cmd_flows[0];
        assert_eq!(flow.name.as_deref(), Some("cmd1"));
        assert_eq!(flow.dest.len(), 1);

        let dest = &flow.dest[0];
        assert_eq!(dest.loc.app, Some("http://example.com:8000".to_string()));
        assert_eq!(dest.loc.extension, Some("ext2".to_string()));

        // Verify the msg_conversion was properly set
        assert!(dest.msg_conversion.is_some());
        let dest_msg_conversion = dest.msg_conversion.as_ref().unwrap();

        // Check conversion type.
        assert_eq!(
            dest_msg_conversion.msg.as_ref().unwrap().conversion_type,
            MsgConversionType::PerProperty
        );

        // Check rules.
        let rules = &dest_msg_conversion.msg.as_ref().unwrap().rules;
        assert_eq!(rules.keep_original, Some(true));
        assert_eq!(rules.rules.len(), 2);

        // Check first rule.
        let rule1 = &rules.rules[0];
        assert_eq!(rule1.path, "ten.name");
        assert_eq!(rule1.conversion_mode, MsgConversionMode::FixedValue);
        assert_eq!(rule1.value.as_ref().unwrap().as_str().unwrap(), "test_value");

        // Check second rule
        let rule2 = &rules.rules[1];
        assert_eq!(rule2.path, "param1");
        assert_eq!(rule2.conversion_mode, MsgConversionMode::FixedValue);
        assert_eq!(rule2.value.as_ref().unwrap().as_i64().unwrap(), 42);
    }

    #[tokio::test]
    async fn test_add_connection_with_fixed_value_msg_conversion_with_required() {
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

        // Create a message conversion with fixed value.
        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "bar".to_string(),
                        conversion_mode: MsgConversionMode::FixedValue,
                        original_path: None,
                        value: Some(serde_json::Value::String("some_string_value".to_string())),
                    }],
                    keep_original: Some(true),
                },
            }),
            result: None,
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
        // Test adding a connection with msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd6".to_string()],
            Some(msg_conversion.clone()),
        )
        .await;

        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_add_connection_with_fixed_value_result_conversion_with_required() {
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

        // Create a message conversion with fixed value.
        let msg_conversion = MsgAndResultConversion {
            msg: None,
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "bar".to_string(),
                        conversion_mode: MsgConversionMode::FixedValue,
                        original_path: None,
                        value: Some(serde_json::Value::String("some_string_value".to_string())),
                    }],
                    keep_original: Some(true),
                },
            }),
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
        // Test adding a connection with msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd8".to_string()],
            Some(msg_conversion),
        )
        .await;
        println!("result: {result:?}");

        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_add_connection_with_from_original_msg_conversion_with_required() {
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

        // Create a message conversion with fixed value.
        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "bar".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("foo".to_string()),
                        value: None,
                    }],
                    keep_original: Some(true),
                },
            }),
            result: None,
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
        // Test adding a connection with msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd6".to_string()],
            Some(msg_conversion.clone()),
        )
        .await;

        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_add_connection_with_from_original_msg_conversion() {
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

        // Create a message conversion with from_original mode.
        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "new_param".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("param1".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
            result: None,
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
        // Test adding a connection with msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            Some(msg_conversion),
        )
        .await;

        println!("result: {result:?}");

        assert!(result.is_ok());
        assert!(graph.connections.is_some());

        // Verify connection and msg_conversion were added correctly.
        let connections = graph.connections.as_ref().unwrap();
        let connection = &connections[0];
        let cmd_flows = connection.cmd.as_ref().unwrap();
        let flow = &cmd_flows[0];
        let dest = &flow.dest[0];

        // Verify the msg_conversion was properly set.
        assert!(dest.msg_conversion.is_some());
        let dest_msg_conversion = dest.msg_conversion.as_ref().unwrap();

        // Check rules.
        let rules = &dest_msg_conversion.msg.as_ref().unwrap().rules;
        assert_eq!(rules.keep_original, Some(false));
        assert_eq!(rules.rules.len(), 1);

        // Check the rule.
        let rule = &rules.rules[0];
        assert_eq!(rule.path, "new_param");
        assert_eq!(rule.conversion_mode, MsgConversionMode::FromOriginal);
        assert_eq!(rule.original_path.as_ref().unwrap(), "param1");
        assert!(rule.value.is_none());
    }

    #[tokio::test]
    async fn test_add_connection_with_msg_and_result_conversion() {
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

        // Create a message conversion with both message and result conversion.
        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "mapped_param".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("param1".to_string()),
                        value: None,
                    }],
                    keep_original: Some(true),
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "mapped_detail".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("detail".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
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
        // Test adding a connection with msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            Some(msg_conversion),
        )
        .await;
        eprintln!("result: {result:?}");

        assert!(result.is_ok());

        // Verify connection and msg_conversion were added correctly.
        let dest = &graph.connections.as_ref().unwrap()[0].cmd.as_ref().unwrap()[0].dest[0];

        // Verify the msg_conversion was properly set.
        let dest_msg_conversion = dest.msg_conversion.as_ref().unwrap();

        // Verify both message and result conversion exist.
        assert!(dest_msg_conversion.result.is_some());

        // Check message conversion.
        assert_eq!(dest_msg_conversion.msg.as_ref().unwrap().rules.rules[0].path, "mapped_param");

        // Check result conversion.
        let result_conversion = dest_msg_conversion.result.as_ref().unwrap();
        assert_eq!(result_conversion.conversion_type, MsgConversionType::PerProperty);
        assert_eq!(result_conversion.rules.rules[0].path, "mapped_detail");
        assert_eq!(result_conversion.rules.rules[0].original_path.as_ref().unwrap(), "detail");
    }

    #[tokio::test]
    async fn test_add_connection_with_invalid_msg_conversion() {
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

        // Create an invalid message conversion with empty rules.
        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![],
                    keep_original: None,
                },
            }),
            result: None,
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
        // Test adding a connection with invalid msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            Some(msg_conversion),
        )
        .await;

        // Should fail validation due to empty rules.
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg.contains("conversion rules are empty"));
    }

    #[tokio::test]
    async fn test_add_connection_with_invalid_forward_conversion_1() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
                create_test_node("ext3", "extension_addon_3", Some("http://example.com:8000")),
                create_test_node("ext4", "addon4", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "param1".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("param2".to_string()),
                        value: None,
                    }],
                    keep_original: None,
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "mapped_detail".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("detail".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext_2".to_string(),
        )
        .unwrap();
        // Test adding a connection with invalid msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            Some(msg_conversion),
        )
        .await;

        println!("result: {result:?}");

        // Should fail validation due to empty rules.
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_add_connection_with_result_conversion_success() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
                create_test_node("ext3", "extension_addon_3", Some("http://example.com:8000")),
                create_test_node("ext4", "addon4", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "param1".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("param1".to_string()),
                        value: None,
                    }],
                    keep_original: None,
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "mapped_detail".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("detail".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
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
        // Test adding a connection with invalid msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            Some(msg_conversion),
        )
        .await;

        println!("result: {result:?}");

        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_add_connection_with_result_conversion_failure() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
                create_test_node("ext3", "extension_addon_3", Some("http://example.com:8000")),
                create_test_node("ext4", "addon4", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "param1".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("param1".to_string()),
                        value: None,
                    }],
                    keep_original: None,
                },
            }),
            result: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "mapped_detail".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("string_detail".to_string()),
                        value: None,
                    }],
                    keep_original: Some(false),
                },
            }),
        };

        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext_2".to_string(),
        )
        .unwrap();
        // Test adding a connection with invalid msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd1".to_string()],
            Some(msg_conversion),
        )
        .await;

        println!("result: {result:?}");

        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_add_connection_with_msg_conversion_without_result_conversion_failure() {
        let mut pkgs_cache = HashMap::new();
        let mut graphs_cache = HashMap::new();

        inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;

        // Create a graph with two nodes.
        let mut graph = Graph {
            nodes: vec![
                create_test_node("ext1", "extension_addon_1", Some("http://example.com:8000")),
                create_test_node("ext2", "extension_addon_2", Some("http://example.com:8000")),
                create_test_node("ext3", "extension_addon_3", Some("http://example.com:8000")),
                create_test_node("ext4", "addon4", Some("http://example.com:8000")),
            ],
            connections: None,
            exposed_messages: None,
            exposed_properties: None,
        };

        let msg_conversion = MsgAndResultConversion {
            msg: Some(MsgConversion {
                conversion_type: MsgConversionType::PerProperty,
                rules: MsgConversionRules {
                    rules: vec![MsgConversionRule {
                        path: "param1".to_string(),
                        conversion_mode: MsgConversionMode::FromOriginal,
                        original_path: Some("param1".to_string()),
                        value: None,
                    }],
                    keep_original: None,
                },
            }),
            result: None,
        };

        let src = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext_1".to_string(),
        )
        .unwrap();
        let dest = GraphLoc::with_app_and_type_and_name(
            Some("http://example.com:8000".to_string()),
            GraphNodeType::Extension,
            "ext_2".to_string(),
        )
        .unwrap();
        // Test adding a connection with invalid msg_conversion.
        let result = graph_add_connection(
            &mut graph,
            &Some(TEST_DIR.to_string()),
            &pkgs_cache,
            src,
            dest,
            MsgType::Cmd,
            vec!["cmd2".to_string()],
            Some(msg_conversion),
        )
        .await;

        println!("result: {result:?}");

        assert!(result.is_err());
    }
}
