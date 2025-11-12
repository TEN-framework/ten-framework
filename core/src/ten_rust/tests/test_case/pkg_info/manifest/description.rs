//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use anyhow::Result;
    use ten_rust::pkg_info::manifest::Manifest;

    #[test]
    fn test_manifest_with_description_field() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-US": {
                        "content": "English description"
                    },
                    "zh-CN": {
                        "content": "中文描述"
                    },
                    "es-ES": {
                        "content": "Descripción en español"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let description = manifest.description.unwrap();
        assert_eq!(
            description.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "English description"
        );
        assert_eq!(description.locales.get("zh-CN").unwrap().content.as_ref().unwrap(), "中文描述");
        assert_eq!(
            description.locales.get("es-ES").unwrap().content.as_ref().unwrap(),
            "Descripción en español"
        );
    }

    #[test]
    fn test_manifest_without_description_field() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0"
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");
        assert!(manifest.description.is_none());
    }

    #[test]
    fn test_manifest_with_invalid_locale_format() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en_US": {
                        "content": "Should fail - using underscore instead of hyphen"
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {error_msg}");
        assert!(
            error_msg.contains("Invalid locale format")
                || error_msg.contains("locale")
                || error_msg.contains("does not match")
                || error_msg.contains("en_US")
        );
    }

    #[test]
    fn test_manifest_with_empty_description_content() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-US": {
                        "content": ""
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {error_msg}");
        assert!(error_msg.contains("cannot be empty"));
    }

    #[test]
    fn test_manifest_with_import_uri_description() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-US": {
                        "import_uri": "http://example.com/desc_en.md"
                    },
                    "zh-CN": {
                        "content": "中文描述"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let description = manifest.description.unwrap();
        assert_eq!(
            description.locales.get("en-US").unwrap().import_uri.as_ref().unwrap(),
            "http://example.com/desc_en.md"
        );
        assert_eq!(description.locales.get("zh-CN").unwrap().content.as_ref().unwrap(), "中文描述");
    }

    #[test]
    fn test_manifest_with_simple_language_codes() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en": {
                        "content": "English description"
                    },
                    "zh": {
                        "content": "中文描述"
                    },
                    "es": {
                        "content": "Descripción en español"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let description = manifest.description.unwrap();
        assert_eq!(
            description.locales.get("en").unwrap().content.as_ref().unwrap(),
            "English description"
        );
        assert_eq!(description.locales.get("zh").unwrap().content.as_ref().unwrap(), "中文描述");
        assert_eq!(
            description.locales.get("es").unwrap().content.as_ref().unwrap(),
            "Descripción en español"
        );
    }

    #[test]
    fn test_manifest_with_mixed_locale_formats() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en": {
                        "content": "English description"
                    },
                    "zh-CN": {
                        "content": "简体中文描述"
                    },
                    "zh-TW": {
                        "content": "繁體中文描述"
                    },
                    "es-ES": {
                        "content": "Descripción en español"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        let description = manifest.description.unwrap();
        assert_eq!(
            description.locales.get("en").unwrap().content.as_ref().unwrap(),
            "English description"
        );
        assert_eq!(
            description.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "简体中文描述"
        );
        assert_eq!(
            description.locales.get("zh-TW").unwrap().content.as_ref().unwrap(),
            "繁體中文描述"
        );
        assert_eq!(
            description.locales.get("es-ES").unwrap().content.as_ref().unwrap(),
            "Descripción en español"
        );
    }

    #[test]
    fn test_manifest_with_invalid_bcp47_formats() {
        // Test with uppercase language code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "EN-US": {
                        "content": "Should fail - uppercase language code"
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());

        // Test with lowercase region code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-us": {
                        "content": "Should fail - lowercase region code"
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());

        // Test with three-letter language code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "eng-US": {
                        "content": "Should fail - three-letter language code"
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
    }

    #[test]
    fn test_manifest_with_both_content_and_import_uri() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-US": {
                        "content": "Test Description",
                        "import_uri": "http://example.com/desc_en.md"
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {error_msg}");
        assert!(error_msg.contains("cannot have both"));
    }

    #[test]
    fn test_manifest_with_neither_content_nor_import_uri() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-US": {}
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {error_msg}");
        assert!(error_msg.contains("must have either"));
    }
}
