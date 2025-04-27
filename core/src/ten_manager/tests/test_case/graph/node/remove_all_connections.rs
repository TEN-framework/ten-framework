//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::fs::{self};
    use std::path::Path;

    use anyhow::Result;
    use serde_json::{json, Map, Value};
    use tempfile::TempDir;

    use ten_manager::graph::update_graph_node_all_fields;
    use ten_manager::json::write_property_json_file;
    use ten_rust::graph::node::GraphNode;
    use ten_rust::pkg_info::constants::PROPERTY_JSON_FILENAME;
    use ten_rust::pkg_info::pkg_type::PkgType;
    use ten_rust::pkg_info::pkg_type_and_name::PkgTypeAndName;

    #[test]
    fn test_update_graph_node_removes_all_connections() -> Result<()> {
        // Create a temporary directory for our test.
        let temp_dir = TempDir::new()?;
        let temp_path = temp_dir.path().to_str().unwrap().to_string();

        // Create a property with a graph that has nodes and connections.
        let mut all_fields = Map::new();
        all_fields.insert("name".to_string(), json!("test-app"));

        // Create ten field with predefined_graphs.
        let mut ten_obj = Map::new();
        let mut graphs = Vec::new();

        // Create a graph with nodes.
        let mut graph1 = Map::new();
        graph1.insert("name".to_string(), json!("test-graph"));

        // Create nodes.
        let mut nodes = Vec::new();
        nodes.push(json!({
            "type": "extension",
            "name": "node1",
            "addon": "addon1"
        }));
        nodes.push(json!({
            "type": "extension",
            "name": "node2",
            "addon": "addon2"
        }));
        graph1.insert("nodes".to_string(), Value::Array(nodes));

        // Create connections where node1 and node2 both connect to each other.
        let mut connections = Vec::new();
        connections.push(json!({
            "extension": "node1",
            "cmd": [{
                "name": "cmd1",
                "dest": [{
                    "extension": "node2"
                }]
            }]
        }));
        connections.push(json!({
            "extension": "node2",
            "data": [{
                "name": "data1",
                "dest": [{
                    "extension": "node1"
                }]
            }]
        }));
        graph1.insert("connections".to_string(), Value::Array(connections));

        graphs.push(Value::Object(graph1));
        ten_obj.insert("predefined_graphs".to_string(), Value::Array(graphs));
        all_fields.insert("ten".to_string(), Value::Object(ten_obj));

        // Write the initial property to the temp directory.
        let property_path = Path::new(&temp_path).join(PROPERTY_JSON_FILENAME);

        write_property_json_file(&temp_path, &all_fields)?;

        // Create nodes to remove (both node1 and node2).
        let remove_nodes = vec![
            GraphNode {
                type_and_name: PkgTypeAndName {
                    pkg_type: PkgType::Extension,
                    name: "node1".to_string(),
                },
                addon: "addon1".to_string(),
                extension_group: None,
                app: None,
                property: None,
            },
            GraphNode {
                type_and_name: PkgTypeAndName {
                    pkg_type: PkgType::Extension,
                    name: "node2".to_string(),
                },
                addon: "addon2".to_string(),
                extension_group: None,
                app: None,
                property: None,
            },
        ];

        // Update the property: remove both nodes.
        update_graph_node_all_fields(
            &temp_path,
            &mut all_fields,
            "test-graph",
            None,
            Some(&remove_nodes),
            None,
        )?;

        // Read the updated property.json file back in to verify changes.
        let updated_property_json = fs::read_to_string(&property_path)?;
        let updated_property: Map<String, Value> =
            serde_json::from_str(&updated_property_json)?;

        // Verify nodes were removed.
        let updated_graph = &updated_property["ten"]["predefined_graphs"][0];
        let updated_nodes = updated_graph["nodes"].as_array().unwrap();
        assert_eq!(updated_nodes.len(), 0);

        // Verify connections field is completely removed.
        assert!(!updated_graph
            .as_object()
            .unwrap()
            .contains_key("connections"));

        Ok(())
    }
}
