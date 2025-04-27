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
    fn test_update_graph_node_removes_connection_destinations() -> Result<()> {
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
        nodes.push(json!({
            "type": "extension",
            "name": "node3",
            "addon": "addon3"
        }));
        graph1.insert("nodes".to_string(), Value::Array(nodes));

        // Create a connection with multiple message types and destinations.
        let mut connections = Vec::new();
        connections.push(json!({
            "extension": "node1",
            "cmd": [{
                "name": "cmd1",
                "dest": [
                    { "extension": "node2" },
                    { "extension": "node3" }
                ]
            }],
            "data": [{
                "name": "data1",
                "dest": [
                    { "extension": "node2" }
                ]
            }]
        }));

        // Add a connection with all message types pointing to node2.
        connections.push(json!({
            "extension": "node3",
            "cmd": [{
                "name": "cmd3",
                "dest": [{ "extension": "node2" }]
            }],
            "data": [{
                "name": "data3",
                "dest": [{ "extension": "node2" }]
            }],
            "audio_frame": [{
                "name": "audio3",
                "dest": [{ "extension": "node2" }]
            }],
            "video_frame": [{
                "name": "video3",
                "dest": [{ "extension": "node2" }]
            }]
        }));

        graph1.insert("connections".to_string(), Value::Array(connections));

        graphs.push(Value::Object(graph1));
        ten_obj.insert("predefined_graphs".to_string(), Value::Array(graphs));
        all_fields.insert("ten".to_string(), Value::Object(ten_obj));

        // Write the initial property to the temp directory.
        let property_path = Path::new(&temp_path).join(PROPERTY_JSON_FILENAME);

        write_property_json_file(&temp_path, &all_fields)?;

        // Create a node to remove (node2).
        let remove_node = GraphNode {
            type_and_name: PkgTypeAndName {
                pkg_type: PkgType::Extension,
                name: "node2".to_string(),
            },
            addon: "addon2".to_string(),
            extension_group: None,
            app: None,
            property: None,
        };
        let remove_nodes = vec![remove_node];

        // Update the property: remove node2.
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

        // Verify nodes were updated.
        let updated_graph = &updated_property["ten"]["predefined_graphs"][0];
        let updated_nodes = updated_graph["nodes"].as_array().unwrap();
        assert_eq!(updated_nodes.len(), 2);

        // Verify connections were updated.
        let updated_connections =
            updated_graph["connections"].as_array().unwrap();

        // There should be 1 connection left (from node1 to node3).
        assert_eq!(updated_connections.len(), 1);
        assert_eq!(updated_connections[0]["extension"], "node1");

        // Check that the cmd flow only has node3 as destination.
        let cmd_flows = updated_connections[0]["cmd"].as_array().unwrap();
        assert_eq!(cmd_flows.len(), 1);
        let cmd_dests = cmd_flows[0]["dest"].as_array().unwrap();
        assert_eq!(cmd_dests.len(), 1);
        assert_eq!(cmd_dests[0]["extension"], "node3");

        // The data flow should be removed since its only destination was node2.
        assert!(!updated_connections[0]
            .as_object()
            .unwrap()
            .contains_key("data"));

        // The connection from node3 should be completely removed since all its
        // destinations were to node2.
        assert_eq!(updated_connections.len(), 1);

        Ok(())
    }
}
