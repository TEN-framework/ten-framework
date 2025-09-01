//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_manager::schema::validate_tman_config;

    // Test empty config - now empty config should be valid.
    #[test]
    fn test_empty_config() {
        let config_json = serde_json::json!({});

        let result = validate_tman_config(&config_json);
        assert!(result.is_ok(), "Empty config should be valid now: {result:?}");
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
        assert!(result.is_ok(), "Should validate a minimal config: {result:?}");
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
        assert!(result.is_ok(), "Should validate a full config: {result:?}");
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
        assert!(result.is_err(), "Should fail when designer section has invalid values");
    }
}
