use serde_json::json;
//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
use ten_manager::designer::storage::in_memory::key_parser::{
    get_value_by_key, parse_key, set_value_by_key, KeySegment,
};

#[test]
fn test_parse_simple_key() {
    let result = parse_key("graph_ui").unwrap();
    assert_eq!(result.len(), 1);
    match &result[0] {
        KeySegment::Object(name) => assert_eq!(name, "graph_ui"),
        _ => panic!("Expected object segment"),
    }
}

#[test]
fn test_parse_nested_key() {
    let result = parse_key("graph_ui.test_graph").unwrap();
    assert_eq!(result.len(), 2);
    match &result[0] {
        KeySegment::Object(name) => assert_eq!(name, "graph_ui"),
        _ => panic!("Expected object segment"),
    }
    match &result[1] {
        KeySegment::Object(name) => assert_eq!(name, "test_graph"),
        _ => panic!("Expected object segment"),
    }
}

#[test]
fn test_parse_array_key() {
    let result = parse_key("nodes[0].position").unwrap();
    assert_eq!(result.len(), 2);
    match &result[0] {
        KeySegment::Array(name, index) => {
            assert_eq!(name, "nodes");
            assert_eq!(*index, 0);
        }
        _ => panic!("Expected array segment"),
    }
    match &result[1] {
        KeySegment::Object(name) => assert_eq!(name, "position"),
        _ => panic!("Expected object segment"),
    }
}

#[test]
fn test_invalid_key_uppercase() {
    assert!(parse_key("Graph_ui").is_err());
}

#[test]
fn test_invalid_key_special_chars() {
    assert!(parse_key("graph-ui").is_err());
}

#[test]
fn test_get_value_simple() {
    let data = json!({"graph_ui": {"test": "value"}});
    let result = get_value_by_key(&data, "graph_ui").unwrap();
    assert_eq!(result, Some(json!({"test": "value"})));
}

#[test]
fn test_get_value_nested() {
    let data = json!({"graph_ui": {"test": "value"}});
    let result = get_value_by_key(&data, "graph_ui.test").unwrap();
    assert_eq!(result, Some(json!("value")));
}

#[test]
fn test_get_value_array() {
    let data = json!({"nodes": [{"x": 10}, {"x": 20}]});
    let result = get_value_by_key(&data, "nodes[1].x").unwrap();
    assert_eq!(result, Some(json!(20)));
}

#[test]
fn test_set_value_simple() {
    let mut data = json!({});
    set_value_by_key(&mut data, "test", json!("value")).unwrap();
    assert_eq!(data, json!({"test": "value"}));
}

#[test]
fn test_set_value_nested() {
    let mut data = json!({});
    set_value_by_key(&mut data, "graph_ui.test", json!("value")).unwrap();
    assert_eq!(data, json!({"graph_ui": {"test": "value"}}));
}

#[test]
fn test_set_value_array() {
    let mut data = json!({});
    set_value_by_key(&mut data, "nodes[1].x", json!(20)).unwrap();
    assert_eq!(data, json!({"nodes": [null, {"x": 20}]}));
}
