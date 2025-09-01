//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

#[cfg(test)]
mod tests {
    use anyhow::Result;
    use ten_rust::graph::Graph;

    #[tokio::test]
    async fn test_expand_names_to_individual_items() -> Result<()> {
        let test_json = r#"{
            "connections": [
                {
                    "app": "http://localhost:8000",
                    "extension": "ext_a",
                    "cmd": [
                        {
                            "names": ["cmd_1", "cmd_2", "cmd_3"],
                            "dest": [
                                {
                                    "extension": "ext_b"
                                }
                            ]
                        }
                    ]
                }
            ]
        }"#;

        // Parse the graph
        let graph: Graph = serde_json::from_str(test_json)?;

        // Expand the names
        let expanded = graph.expand_names_to_individual_items()?;

        assert!(expanded.is_some(), "Expected expansion to occur");

        let expanded_graph = expanded.unwrap();
        println!("{}", serde_json::to_string_pretty(&expanded_graph).unwrap());

        // Check that we have the right number of cmd flows
        let connections = expanded_graph.connections.as_ref().unwrap();
        assert_eq!(connections.len(), 1);

        let cmd_flows = connections[0].cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 3, "Expected 3 cmd flows after expansion");

        // Check that the names are correct
        let mut names: Vec<&str> = cmd_flows.iter().map(|f| f.name.as_deref().unwrap()).collect();
        names.sort();
        assert_eq!(names, vec!["cmd_1", "cmd_2", "cmd_3"]);

        // Check that none of the flows have the names field anymore
        for flow in cmd_flows {
            assert!(flow.names.is_none(), "names field should be None after expansion");
            assert_eq!(flow.dest.len(), 1, "dest should be preserved");
            assert_eq!(flow.dest[0].loc.extension.as_ref().unwrap(), "ext_b");
        }

        println!("Expansion test passed successfully!");
        println!("Original had 1 cmd flow with names array");
        println!("Expanded to {} cmd flows with individual names", cmd_flows.len());

        Ok(())
    }

    #[tokio::test]
    async fn test_expand_names_with_data_flows() -> Result<()> {
        let test_json = r#"{
            "connections": [
                {
                    "extension": "ext_a",
                    "data": [
                        {
                            "names": ["data_1", "data_2"],
                            "dest": [
                                {
                                    "extension": "ext_b"
                                }
                            ]
                        }
                    ]
                }
            ]
        }"#;

        let graph: Graph = serde_json::from_str(test_json)?;
        let expanded = graph.expand_names_to_individual_items()?;

        assert!(expanded.is_some());
        let expanded_graph = expanded.unwrap();
        println!("{}", serde_json::to_string_pretty(&expanded_graph).unwrap());

        let connections = expanded_graph.connections.as_ref().unwrap();
        let data_flows = connections[0].data.as_ref().unwrap();
        assert_eq!(data_flows.len(), 2);

        let mut names: Vec<&str> = data_flows.iter().map(|f| f.name.as_deref().unwrap()).collect();
        names.sort();
        assert_eq!(names, vec!["data_1", "data_2"]);

        Ok(())
    }

    #[tokio::test]
    async fn test_no_expansion_needed() -> Result<()> {
        let test_json = r#"{
            "connections": [
                {
                    "extension": "ext_a",
                    "cmd": [
                        {
                            "name": "cmd_1",
                            "dest": [
                                {
                                    "extension": "ext_b"
                                }
                            ]
                        }
                    ]
                }
            ]
        }"#;

        let graph: Graph = serde_json::from_str(test_json)?;
        let expanded = graph.expand_names_to_individual_items()?;

        assert!(
            expanded.is_none(),
            "No expansion should be needed when no names fields are present"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_flatten_graph_includes_names_expansion() -> Result<()> {
        let test_json = r#"{
            "connections": [
                {
                    "extension": "ext_a",
                    "cmd": [
                        {
                            "names": ["cmd_1", "cmd_2"],
                            "dest": [
                                {
                                    "extension": "ext_b"
                                }
                            ]
                        }
                    ]
                }
            ]
        }"#;

        let graph: Graph = serde_json::from_str(test_json)?;
        let flattened = graph.flatten_graph(None).await?;

        assert!(flattened.is_some(), "flatten_graph should return expanded graph");
        let flattened_graph = flattened.unwrap();
        println!("{}", serde_json::to_string_pretty(&flattened_graph).unwrap());

        let connections = flattened_graph.connections.as_ref().unwrap();
        let cmd_flows = connections[0].cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 2, "flatten_graph should include names expansion");

        Ok(())
    }

    #[tokio::test]
    async fn test_flatten_graph_includes_names_expansion_with_app() -> Result<()> {
        let test_json = r#"{
            "connections": [
                {
                    "app": "msgpack://127.0.0.1:8001/",
                    "extension": "ext_a",
                    "cmd": [
                        {
                            "names": ["cmd_1", "cmd_2"],
                            "dest": [
                                {
                                    "app": "msgpack://127.0.0.1:8001/",
                                    "extension": "ext_b"
                                }
                            ]
                        }
                    ]
                }
            ]
        }"#;

        let graph: Graph = serde_json::from_str(test_json)?;
        let flattened = graph.flatten_graph(None).await?;

        assert!(flattened.is_some(), "flatten_graph should return expanded graph");
        let flattened_graph = flattened.unwrap();
        println!("{}", serde_json::to_string_pretty(&flattened_graph).unwrap());

        let connections = flattened_graph.connections.as_ref().unwrap();
        let cmd_flows = connections[0].cmd.as_ref().unwrap();
        assert_eq!(cmd_flows.len(), 2, "flatten_graph should include names expansion");

        Ok(())
    }

    #[tokio::test]
    async fn test_duplicate_names_error_between_name_and_names_fields() -> Result<()> {
        let test_json = r#"{
            "connections": [
                {
                    "extension": "ext_a",
                    "data": [
                        {
                            "name": "data_1",
                            "dest": [
                                {
                                    "extension": "ext_b"
                                }
                            ]
                        },
                        {
                            "names": ["data_1", "data_2"],
                            "dest": [
                                {
                                    "extension": "ext_b"
                                }
                            ]
                        }
                    ]
                }
            ]
        }"#;

        let graph: Graph = serde_json::from_str(test_json)?;

        // Test that check_message_names correctly identifies the duplicate
        let result = graph.check_message_names();

        assert!(result.is_err(), "Expected error due to duplicate data name 'data_1'");

        let error_msg = result.unwrap_err().to_string();
        assert!(
            error_msg.contains("data_1"),
            "Error message should mention the duplicate name 'data_1'"
        );
        assert!(
            error_msg.contains("flow[0]") && error_msg.contains("flow[1]"),
            "Error message should mention both flow indices where 'data_1' appears"
        );

        println!("Successfully detected duplicate name error: {error_msg}");

        Ok(())
    }
}
