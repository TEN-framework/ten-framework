//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use anyhow::Result;
    use serde_json::Value;
    use tempfile::TempDir;

    use ten_manager::graph::update_graph_connections_in_property_all_fields;
    use ten_rust::graph::connection::{
        GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow,
    };
    use ten_rust::graph::msg_conversion::{
        MsgAndResultConversion, MsgConversion, MsgConversionMode,
        MsgConversionRule, MsgConversionRules, MsgConversionType,
    };
    use ten_rust::pkg_info::constants::PROPERTY_JSON_FILENAME;

    #[test]
    fn test_modify_msg_conversion() -> Result<()> {
        // Create a temporary directory for our test.
        let temp_dir = TempDir::new()?;
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Initial property.json with a connection that has a msg_conversion.
        let initial_json = include_str!(
            "../../../test_data/initial_property_with_msg_conversion.json"
        );

        // Expected JSON after modifying the msg_conversion.
        let expected_json = include_str!(
            "../../../test_data/\
             expected_property_with_modified_msg_conversion.json"
        );

        // Write the initial JSON to property.json.
        let property_path =
            std::path::Path::new(&test_dir).join(PROPERTY_JSON_FILENAME);
        std::fs::write(&property_path, initial_json)?;

        // Parse the initial JSON to all_fields.
        let mut all_fields: serde_json::Map<String, Value> =
            serde_json::from_str(initial_json)?;

        // Create a connection with modified message conversion.
        let connection_to_modify = GraphConnection {
            loc: GraphLoc {
                app: Some("http://example.com:8000".to_string()),
                extension: Some("extension_1".to_string()),
                subgraph: None,
                selector: None,
            },
            cmd: Some(vec![GraphMessageFlow::new(
                "existing_cmd".to_string(),
                vec![GraphDestination {
                    loc: GraphLoc {
                        app: Some("http://example.com:8000".to_string()),
                        extension: Some("extension_2".to_string()),
                        subgraph: None,
                        selector: None,
                    },
                    msg_conversion: Some(MsgAndResultConversion {
                        msg: Some(MsgConversion {
                            conversion_type: MsgConversionType::PerProperty,
                            rules: MsgConversionRules {
                                rules: vec![
                                    MsgConversionRule {
                                        path: "new_param".to_string(),
                                        conversion_mode:
                                            MsgConversionMode::FromOriginal,
                                        original_path: Some(
                                            "new_source_param".to_string(),
                                        ),
                                        value: None,
                                    },
                                    MsgConversionRule {
                                        path: "fixed_param".to_string(),
                                        conversion_mode:
                                            MsgConversionMode::FixedValue,
                                        original_path: None,
                                        value: Some(serde_json::json!(100)),
                                    },
                                ],
                                keep_original: Some(false),
                            },
                        }),
                        result: Some(MsgConversion {
                            conversion_type: MsgConversionType::PerProperty,
                            rules: MsgConversionRules {
                                rules: vec![MsgConversionRule {
                                    path: "result_param".to_string(),
                                    conversion_mode:
                                        MsgConversionMode::FromOriginal,
                                    original_path: Some(
                                        "original_result".to_string(),
                                    ),
                                    value: None,
                                }],
                                keep_original: None,
                            },
                        }),
                    }),
                }],
                vec![],
            )]),
            data: None,
            audio_frame: None,
            video_frame: None,
        };

        let connections_to_modify = vec![connection_to_modify];

        // Update the connections in memory and in the file, specifying only
        // connections_to_modify_msg_conversion.
        update_graph_connections_in_property_all_fields(
            &test_dir,
            &mut all_fields,
            "test_graph",
            None,
            None,
            Some(&connections_to_modify),
        )?;

        // Read the updated property.json.
        let actual_json = std::fs::read_to_string(&property_path)?;

        // Normalize both JSON strings (parse and reformat to remove whitespace
        // differences).
        let expected_value: serde_json::Value =
            serde_json::from_str(expected_json)?;
        let actual_value: serde_json::Value =
            serde_json::from_str(&actual_json)?;

        assert_eq!(
            expected_value, actual_value,
            "Updated property does not match expected property"
        );

        Ok(())
    }
}
