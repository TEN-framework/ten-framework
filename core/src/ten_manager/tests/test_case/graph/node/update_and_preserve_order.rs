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
    fn test_update_graph_node_preserves_field_order() -> Result<()> {
        // Create a temporary directory for our test.
        let temp_dir = TempDir::new()?;
        let temp_path = temp_dir.path().to_str().unwrap().to_string();

        // Create a property with fields in a specific order.
        let mut all_fields = Map::new();
        all_fields.insert("name".to_string(), json!("test-app"));
        all_fields.insert("version".to_string(), json!("1.0.0"));
        all_fields.insert("description".to_string(), json!("Test App"));

        // Create ten field with predefined_graphs.
        let mut ten_obj = Map::new();
        let mut graphs = Vec::new();

        // Create a graph with nodes.
        let mut graph1 = Map::new();
        graph1.insert("name".to_string(), json!("test-graph"));
        graph1.insert("auto_start".to_string(), json!(true));

        // Initial nodes with specific order.
        let mut nodes = Vec::new();
        nodes.push(json!({
            "type": "extension",
            "name": "first-node",
            "addon": "first-addon"
        }));
        nodes.push(json!({
            "type": "extension",
            "name": "second-node",
            "addon": "second-addon"
        }));
        nodes.push(json!({
            "type": "extension",
            "name": "third-node",
            "addon": "third-addon"
        }));
        graph1.insert("nodes".to_string(), Value::Array(nodes));

        // Add connections array with connections between the nodes.
        let mut connections = Vec::new();

        // Add a connection from first-node to second-node.
        connections.push(json!({
            "extension": "first-node",
            "cmd": [{
                "name": "test-cmd",
                "dest": [{
                    "extension": "second-node"
                }]
            }]
        }));

        // Add a connection from second-node to third-node and first-node.
        connections.push(json!({
            "extension": "second-node",
            "cmd": [{
                "name": "test-cmd",
                "dest": [{
                    "extension": "third-node"
                }, {
                    "extension": "first-node"
                }]
            }]
        }));

        // Add a connection from third-node to first-node.
        connections.push(json!({
            "extension": "third-node",
            "data": [{
                "name": "test-data",
                "dest": [{
                    "extension": "first-node"
                }]
            }]
        }));

        graph1.insert("connections".to_string(), Value::Array(connections));

        graphs.push(Value::Object(graph1));
        ten_obj.insert("predefined_graphs".to_string(), Value::Array(graphs));
        all_fields.insert("ten".to_string(), Value::Object(ten_obj));

        // Add more fields after ten.
        all_fields.insert("license".to_string(), json!("Apache-2.0"));
        all_fields.insert("author".to_string(), json!("Test Author"));

        // Write the initial property to the temp directory.
        let property_path = Path::new(&temp_path).join(PROPERTY_JSON_FILENAME);

        write_property_json_file(&temp_path, &all_fields)?;

        // Create a new node to add.
        let new_node = GraphNode {
            type_and_name: PkgTypeAndName {
                pkg_type: PkgType::Extension,
                name: "new-node".to_string(),
            },
            addon: "new-addon".to_string(),
            extension_group: None,
            app: None,
            property: None,
        };
        let new_nodes = vec![new_node];

        // Create a node to remove (the second one).
        let remove_node = GraphNode {
            type_and_name: PkgTypeAndName {
                pkg_type: PkgType::Extension,
                name: "second-node".to_string(),
            },
            addon: "second-addon".to_string(),
            extension_group: None,
            app: None,
            property: None,
        };
        let remove_nodes = vec![remove_node];

        // Update the property: add one node and remove one node in a single
        // call.
        update_graph_node_all_fields(
            &temp_path,
            &mut all_fields,
            "test-graph",
            Some(&new_nodes),
            Some(&remove_nodes),
            None,
        )?;

        // Read the updated property.json file back in to verify changes.
        let updated_property_json = fs::read_to_string(&property_path)?;
        let updated_property: Map<String, Value> =
            serde_json::from_str(&updated_property_json)?;

        // Verify order of top-level fields.
        let field_names: Vec<&String> = updated_property.keys().collect();
        assert_eq!(field_names[0], "name");
        assert_eq!(field_names[1], "version");
        assert_eq!(field_names[2], "description");
        assert_eq!(field_names[3], "ten");
        assert_eq!(field_names[4], "license");
        assert_eq!(field_names[5], "author");

        // Verify nodes were updated correctly.
        let updated_graph = &updated_property["ten"]["predefined_graphs"][0];
        let updated_nodes = updated_graph["nodes"].as_array().unwrap();

        // Check nodes order (first, third, new).
        assert_eq!(updated_nodes.len(), 3);
        assert_eq!(updated_nodes[0]["name"], "first-node");
        assert_eq!(updated_nodes[1]["name"], "third-node");
        assert_eq!(updated_nodes[2]["name"], "new-node");

        // Verify connections were updated correctly.
        let updated_connections =
            updated_graph["connections"].as_array().unwrap();

        // We should now have 1 connection instead of 3 (the third-node
        // connection is the only one left).
        assert_eq!(updated_connections.len(), 1);

        // The remaining connection should be from third-node.
        let third_conn = &updated_connections[0];
        assert_eq!(third_conn["extension"], "third-node");
        assert!(third_conn["data"].as_array().is_some());
        assert_eq!(third_conn["data"][0]["name"], "test-data");
        assert_eq!(third_conn["data"][0]["dest"][0]["extension"], "first-node");

        // The connections from first-node and second-node should be gone
        // - first-node connection had only second-node as destination, so it's
        //   removed.
        // - second-node connection was removed explicitly.

        Ok(())
    }
}
