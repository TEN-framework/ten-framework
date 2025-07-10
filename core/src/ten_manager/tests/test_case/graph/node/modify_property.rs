//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::fs;
    use std::io::Write;
    use std::path::PathBuf;

    use anyhow::Result;
    use serde_json::{Map, Value};
    use tempfile::TempDir;

    use ten_manager::graph::update_graph_node_all_fields;
    use ten_rust::graph::node::GraphNode;

    #[test]
    fn test_update_graph_node_modifies_node_property() -> Result<()> {
        // Create a temporary directory for testing.
        let temp_dir = TempDir::new()?;
        let temp_path = temp_dir.path().to_string_lossy().to_string();
        let property_path = PathBuf::from(&temp_path).join("property.json");

        // Create an initial property.json with a graph that has three nodes.
        let property_json_str = include_str!(
            "../../../test_data/initial_update_graph_node_property.json"
        );

        let mut property_file = fs::File::create(&property_path)?;
        property_file.write_all(property_json_str.as_bytes())?;

        // Parse the property JSON for updates.
        let mut all_fields: Map<String, Value> =
            serde_json::from_str(property_json_str)?;

        // Create nodes with modified properties to update existing nodes.
        let nodes_to_modify = vec![
            // Node1 with modified property
            GraphNode::new_extension_node(
                "node1".to_string(),
                "addon1".to_string(),
                None,
                None,
                Some(serde_json::json!({
                    "setting1": "updated-value1",
                    "setting2": 99,
                    "newSetting": "added-value"
                })),
            ),
            // Node2 with modified property
            GraphNode::new_extension_node(
                "node2".to_string(),
                "addon2".to_string(),
                Some("group1".to_string()),
                None,
                Some(serde_json::json!({
                    "config": "updated-config"
                })),
            ),
            // Non-existent node (should not affect anything)
            GraphNode::new_extension_node(
                "non-existent".to_string(),
                "addon4".to_string(),
                None,
                None,
                Some(serde_json::json!({
                    "test": "value"
                })),
            ),
            // Node with mismatched app field (should not update)
            GraphNode::new_extension_node(
                "node3".to_string(),
                "addon3".to_string(),
                None,
                Some("app2".to_string()), // Mismatched app
                Some(serde_json::json!({
                    "enabled": false
                })),
            ),
        ];

        // Update the property: modify node properties.
        update_graph_node_all_fields(
            &temp_path,
            &mut all_fields,
            "test_graph",
            None,
            None,
            Some(&nodes_to_modify),
        )?;

        // Read the updated property.json file back in to verify changes.
        let updated_property_json_str = fs::read_to_string(&property_path)?;
        let updated_property: Map<String, Value> =
            serde_json::from_str(&updated_property_json_str)?;

        let expected_property_json_str = include_str!(
            "../../../test_data/expected_update_graph_node_property.json"
        );
        let expected_property: Map<String, Value> =
            serde_json::from_str(expected_property_json_str)?;

        // Compare the normalized JSON values.
        assert_eq!(
            updated_property,
            expected_property,
            "Property file doesn't match expected \
             content.\nExpected:\n{}\nActual:\n{}",
            serde_json::to_string_pretty(&updated_property).unwrap(),
            serde_json::to_string_pretty(&expected_property).unwrap()
        );

        Ok(())
    }
}
