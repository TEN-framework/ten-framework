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
    fn test_manifest_with_display_name_field() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "locales": {
                    "en-US": {
                        "content": "Test Extension"
                    },
                    "zh-CN": {
                        "content": "测试扩展"
                    },
                    "es-ES": {
                        "content": "Extensión de Prueba"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let display_name = manifest.display_name.unwrap();
        assert_eq!(
            display_name.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );
        assert_eq!(
            display_name.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "测试扩展"
        );
        assert_eq!(
            display_name.locales.get("es-ES").unwrap().content.as_ref().unwrap(),
            "Extensión de Prueba"
        );
    }

    #[test]
    fn test_manifest_without_display_name_field() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0"
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");
        assert!(manifest.display_name.is_none());
    }

    #[test]
    fn test_manifest_with_invalid_locale_format_in_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
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
    fn test_manifest_with_empty_display_name_content() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
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
    fn test_manifest_with_import_uri_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "locales": {
                    "en-US": {
                        "import_uri": "file://display_name_en.txt"
                    },
                    "zh-CN": {
                        "content": "测试扩展"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let display_name = manifest.display_name.unwrap();
        assert_eq!(
            display_name.locales.get("en-US").unwrap().import_uri.as_ref().unwrap(),
            "file://display_name_en.txt"
        );
        assert_eq!(
            display_name.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "测试扩展"
        );
    }

    #[test]
    fn test_manifest_with_simple_language_codes_in_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "locales": {
                    "en": {
                        "content": "Test Extension"
                    },
                    "zh": {
                        "content": "测试扩展"
                    },
                    "es": {
                        "content": "Extensión de Prueba"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let display_name = manifest.display_name.unwrap();
        assert_eq!(
            display_name.locales.get("en").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );
        assert_eq!(display_name.locales.get("zh").unwrap().content.as_ref().unwrap(), "测试扩展");
        assert_eq!(
            display_name.locales.get("es").unwrap().content.as_ref().unwrap(),
            "Extensión de Prueba"
        );
    }

    #[test]
    fn test_manifest_with_mixed_locale_formats_in_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "locales": {
                    "en": {
                        "content": "Test Extension"
                    },
                    "zh-CN": {
                        "content": "测试扩展"
                    },
                    "zh-TW": {
                        "content": "測試擴展"
                    },
                    "es-ES": {
                        "content": "Extensión de Prueba"
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        let display_name = manifest.display_name.unwrap();
        assert_eq!(
            display_name.locales.get("en").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );
        assert_eq!(
            display_name.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "测试扩展"
        );
        assert_eq!(
            display_name.locales.get("zh-TW").unwrap().content.as_ref().unwrap(),
            "測試擴展"
        );
        assert_eq!(
            display_name.locales.get("es-ES").unwrap().content.as_ref().unwrap(),
            "Extensión de Prueba"
        );
    }

    #[test]
    fn test_manifest_with_invalid_bcp47_formats_in_display_name() {
        // Test with uppercase language code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
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
            "display_name": {
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
            "display_name": {
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
            "display_name": {
                "locales": {
                    "en-US": {
                        "content": "Test Extension",
                        "import_uri": "file://display_name_en.txt"
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
            "display_name": {
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

    #[test]
    fn test_manifest_with_both_description_and_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-US": {
                        "content": "This is a test extension for demonstration purposes"
                    },
                    "zh-CN": {
                        "content": "这是用于演示目的的测试扩展"
                    }
                }
            },
            "display_name": {
                "locales": {
                    "en-US": {
                        "content": "Test Extension"
                    },
                    "zh-CN": {
                        "content": "测试扩展"
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
            "This is a test extension for demonstration purposes"
        );
        assert_eq!(
            description.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "这是用于演示目的的测试扩展"
        );

        let display_name = manifest.display_name.unwrap();
        assert_eq!(
            display_name.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );
        assert_eq!(
            display_name.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "测试扩展"
        );
    }
}
