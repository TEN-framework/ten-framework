//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod predefined_graph;

#[cfg(test)]
mod tests {
    use std::{
        collections::HashMap,
        fs::{self},
    };

    use tempfile::tempdir;
    use ten_rust::{
        graph::msg_conversion::{MsgConversionMode, MsgConversionType},
        pkg_info::{localhost, property::parse_property_from_str},
    };

    #[test]
    fn test_parse_property_with_known_fields() {
        let json_data = r#"
        {
            "ten": {
                "predefined_graphs": [],
                "uri": "http://example.com"
            }
        }
        "#;

        let mut graphs_cache = HashMap::new();

        let property = parse_property_from_str(
            json_data,
            &mut graphs_cache,
            None,
            None,
            None,
        )
        .unwrap();

        assert!(property.ten.is_some());
        let ten_in_property = property.ten.unwrap();

        assert_eq!(ten_in_property.uri.unwrap(), "http://example.com");
        assert!(graphs_cache.is_empty());
        assert_eq!(property.all_fields.len(), 1); // Should contain ten field.
        assert!(property.all_fields.contains_key("ten"));
    }

    #[test]
    fn test_parse_property_with_additional_fields() {
        let json_data = r#"
        {
            "ten": {
                "predefined_graphs": [],
                "uri": "http://example.com"
            },
            "global_field_1": "global_value1"
        }
        "#;

        let mut graphs_cache = HashMap::new();

        let property = parse_property_from_str(
            json_data,
            &mut graphs_cache,
            None,
            None,
            None,
        )
        .unwrap();

        assert!(property.ten.is_some());
        let ten_in_property = property.ten.unwrap();
        assert_eq!(ten_in_property.uri.unwrap(), "http://example.com");
        assert!(graphs_cache.is_empty());

        // Should contain ten and global_field_1.
        assert_eq!(property.all_fields.len(), 2);
        assert_eq!(
            property.all_fields.get("global_field_1").unwrap(),
            "global_value1"
        );
    }

    #[test]
    fn test_dump_property_without_localhost_app_in_graph() {
        let mut graphs_cache = HashMap::new();

        let json_str = include_str!("../../../test_data/property.json");

        let property = parse_property_from_str(
            json_str,
            &mut graphs_cache,
            None,
            None,
            None,
        )
        .unwrap();
        assert!(property.ten.is_some());

        let (_, graph_info) = graphs_cache.into_iter().next().unwrap();

        let nodes = &graph_info.graph.nodes;
        let node = nodes.first().unwrap();
        assert_eq!(node.get_app_uri(), &None);

        let dir = tempdir().unwrap();
        let file_path = dir.path().join("property.json");
        property.dump_property_to_file(&file_path).unwrap();

        let saved_content = fs::read_to_string(file_path).unwrap();
        eprintln!("{}", saved_content);
        assert_eq!(saved_content.find(localhost()), None);
    }

    #[test]
    fn test_dump_property_with_msg_conversion() {
        let prop_str = include_str!(
            "../../../test_data/dump_property_with_msg_conversion.json"
        );

        let mut graphs_cache = HashMap::new();

        let property = parse_property_from_str(
            prop_str,
            &mut graphs_cache,
            None,
            None,
            None,
        )
        .unwrap();
        assert!(property.ten.is_some());

        let (_, graph_info) = graphs_cache.into_iter().next().unwrap();

        let connections = &graph_info.graph.connections.as_ref().unwrap();
        let connection = connections.first().unwrap();
        let cmd = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd.len(), 1);

        let cmd_dest = &cmd.first().unwrap().dest;
        assert_eq!(cmd_dest.len(), 1);

        let msg_conversion =
            cmd_dest.first().unwrap().msg_conversion.as_ref().unwrap();
        assert_eq!(
            msg_conversion.msg.as_ref().unwrap().conversion_type,
            MsgConversionType::PerProperty
        );

        let rules = &msg_conversion.msg.as_ref().unwrap().rules;
        assert!(rules.keep_original.is_none());
        assert_eq!(rules.rules.len(), 2);

        let rule = rules.rules.first().unwrap();
        assert_eq!(rule.conversion_mode, MsgConversionMode::FixedValue);
        assert!(rule.original_path.is_none());
        assert_eq!(rule.value.as_ref().unwrap(), "hello_mapping");

        let dir = tempdir().unwrap();
        let file_path = dir.path().join("property.json");

        property.dump_property_to_file(&file_path).unwrap();

        let saved_content = fs::read_to_string(file_path).unwrap();
        eprintln!("{}", saved_content);
        assert!(saved_content.contains("msg_conversion"));
    }
}
