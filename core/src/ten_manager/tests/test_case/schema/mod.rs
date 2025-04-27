//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_manager::schema::{
        get_designer_schema, validate_designer_config, validate_tman_config,
    };

    // Test empty config - now empty config should be valid.
    #[test]
    fn test_empty_config() {
        let config_json = serde_json::json!({});

        let result = validate_tman_config(&config_json);
        assert!(
            result.is_ok(),
            "Empty config should be valid now: {:?}",
            result
        );
    }

    // Test minimal valid config.
    #[test]
    fn test_minimal_valid_config() {
        let config_json = serde_json::json!({
            "registry": {
                "default": {
                    "index": "https://test-registry.com"
                }
            }
        });

        let result = validate_tman_config(&config_json);
        assert!(
            result.is_ok(),
            "Should validate a minimal config: {:?}",
            result
        );
    }

    // Test full valid config.
    #[test]
    fn test_full_valid_config() {
        let config_json = serde_json::json!({
            "registry": {
                "default": {
                    "index": "https://test-registry.com"
                }
            },
            "admin_token": "admin-token",
            "user_token": "user-token",
            "enable_package_cache": true,
            "designer": {
                "logviewer_line_size": 2000
            }
        });

        let result = validate_tman_config(&config_json);
        assert!(result.is_ok(), "Should validate a full config: {:?}", result);
    }

    // Test case for invalid field type.
    #[test]
    fn test_invalid_field_type() {
        let config_json = serde_json::json!({
            "registry": "not-an-object", // Should be an object.
            "admin_token": "admin-token"
        });

        let result = validate_tman_config(&config_json);
        assert!(result.is_err(), "Should fail with invalid field type");
    }

    // Test case for empty designer config - since the required field was
    // removed, empty config should be valid.
    #[test]
    fn test_empty_designer_config() {
        let designer_json = serde_json::json!({});

        let result = validate_designer_config(&designer_json);
        assert!(
            result.is_ok(),
            "Empty designer config should be valid now: {:?}",
            result
        );
    }

    // Test case for valid designer config.
    #[test]
    fn test_valid_designer_config() {
        let designer_json = serde_json::json!({
            "logviewer_line_size": 2000
        });

        let result = validate_designer_config(&designer_json);
        assert!(
            result.is_ok(),
            "Should validate a valid designer config: {:?}",
            result
        );
    }

    // Test case for minimum value constraint in designer config.
    #[test]
    fn test_designer_config_minimum_constraint() {
        let designer_json = serde_json::json!({
            "logviewer_line_size": 50 // Below minimum of 100.
        });

        let result = validate_designer_config(&designer_json);
        assert!(
            result.is_err(),
            "Should fail when logviewer_line_size is below minimum"
        );
    }

    // Test case for invalid field type in designer config.
    #[test]
    fn test_designer_config_invalid_field_type() {
        let designer_json = serde_json::json!({
            "logviewer_line_size": "not-a-number" // Should be an integer.
        });

        let result = validate_designer_config(&designer_json);
        assert!(
            result.is_err(),
            "Should fail when logviewer_line_size is not an integer"
        );
    }

    // Test case for designer section in full config.
    #[test]
    fn test_designer_section_in_full_config() {
        let config_json = serde_json::json!({
            "registry": {
                "default": {
                    "index": "https://test-registry.com"
                }
            },
            "designer": {
                "logviewer_line_size": 50 // Below minimum of 100.
            }
        });

        let result = validate_tman_config(&config_json);
        assert!(
            result.is_err(),
            "Should fail when designer section has invalid values"
        );
    }

    // Test case for additional properties.
    #[test]
    fn test_additional_properties() {
        let config_json = serde_json::json!({
            "registry": {
                "default": {
                    "index": "https://test-registry.com"
                }
            },
            "unknown_field": "some value", // Additional field.
            "designer": {
                "logviewer_line_size": 2000,
                "unknown_designer_field": true // Additional field in designer section.
            }
        });

        let result = validate_tman_config(&config_json);
        // Additional fields should cause validation failure.
        assert!(result.is_err(), "Should reject additional properties");

        let designer_json = serde_json::json!({
            "logviewer_line_size": 2000,
            "unknown_designer_field": true // Additional field in designer section.
        });

        let result = validate_designer_config(&designer_json);
        assert!(
            result.is_err(),
            "Should reject additional properties in designer schema"
        );
    }

    // Test case for getting designer schema function.
    #[test]
    fn test_get_designer_schema() {
        let schema_str = get_designer_schema();
        assert!(!schema_str.is_empty(), "Designer schema should not be empty");

        // Verify the returned JSON is valid.
        let schema_result =
            serde_json::from_str::<serde_json::Value>(schema_str);
        assert!(schema_result.is_ok(), "Designer schema should be valid JSON");

        // Verify it contains expected key fields.
        let schema_json = schema_result.unwrap();
        assert!(schema_json.is_object(), "Schema should be a JSON object");
        assert!(
            schema_json.get("type").is_some(),
            "Schema should have 'type' field"
        );
        assert!(
            schema_json.get("properties").is_some(),
            "Schema should have 'properties' field"
        );

        // Verify it contains logviewer_line_size property.
        if let Some(props) = schema_json.get("properties") {
            assert!(
                props.get("logviewer_line_size").is_some(),
                "Schema should have logviewer_line_size property"
            );
        } else {
            panic!("Properties field not found in schema");
        }

        // Verify there is no required field (or it is empty).
        let required = schema_json.get("required");
        assert!(
            required.is_none()
                || required.unwrap().as_array().unwrap().is_empty(),
            "Schema should not have 'required' field anymore"
        );
    }
}
