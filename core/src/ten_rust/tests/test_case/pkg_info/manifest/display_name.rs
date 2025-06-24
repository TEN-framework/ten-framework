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
                "en-US": "Test Extension",
                "zh-CN": "测试扩展",
                "es-ES": "Extensión de Prueba"
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let display_name = manifest.display_name.unwrap();
        assert_eq!(display_name.get("en-US").unwrap(), "Test Extension");
        assert_eq!(display_name.get("zh-CN").unwrap(), "测试扩展");
        assert_eq!(display_name.get("es-ES").unwrap(), "Extensión de Prueba");
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
                "en_US": "Should fail - using underscore instead of hyphen"
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
    fn test_manifest_with_empty_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "en-US": ""
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {error_msg}");
        assert!(error_msg.contains("cannot be empty"));
    }

    #[test]
    fn test_manifest_with_simple_language_codes_in_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "en": "Test Extension",
                "zh": "测试扩展",
                "es": "Extensión de Prueba"
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let display_name = manifest.display_name.unwrap();
        assert_eq!(display_name.get("en").unwrap(), "Test Extension");
        assert_eq!(display_name.get("zh").unwrap(), "测试扩展");
        assert_eq!(display_name.get("es").unwrap(), "Extensión de Prueba");
    }

    #[test]
    fn test_manifest_with_mixed_locale_formats_in_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "en": "Test Extension",
                "zh-CN": "测试扩展",
                "zh-TW": "測試擴展",
                "es-ES": "Extensión de Prueba"
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        let display_name = manifest.display_name.unwrap();
        assert_eq!(display_name.get("en").unwrap(), "Test Extension");
        assert_eq!(display_name.get("zh-CN").unwrap(), "测试扩展");
        assert_eq!(display_name.get("zh-TW").unwrap(), "測試擴展");
        assert_eq!(display_name.get("es-ES").unwrap(), "Extensión de Prueba");
    }

    #[test]
    fn test_manifest_with_invalid_bcp47_formats_in_display_name() {
        // Test with uppercase language code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "EN-US": "Should fail - uppercase language code"
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
                "en-us": "Should fail - lowercase region code"
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
                "eng-US": "Should fail - three-letter language code"
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
    }

    #[test]
    fn test_manifest_with_both_description_and_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "en-US": "This is a test extension for demonstration purposes",
                "zh-CN": "这是用于演示目的的测试扩展"
            },
            "display_name": {
                "en-US": "Test Extension",
                "zh-CN": "测试扩展"
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let description = manifest.description.unwrap();
        assert_eq!(
            description.get("en-US").unwrap(),
            "This is a test extension for demonstration purposes"
        );
        assert_eq!(
            description.get("zh-CN").unwrap(),
            "这是用于演示目的的测试扩展"
        );

        let display_name = manifest.display_name.unwrap();
        assert_eq!(display_name.get("en-US").unwrap(), "Test Extension");
        assert_eq!(display_name.get("zh-CN").unwrap(), "测试扩展");
    }
}
