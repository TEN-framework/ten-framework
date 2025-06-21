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
                "en-US": "English description",
                "zh-CN": "中文描述",
                "es-ES": "Descripción en español"
            }
        }"#;

        let manifest: Manifest = manifest_json.parse().unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let description = manifest.description.unwrap();
        assert_eq!(description.get("en-US").unwrap(), "English description");
        assert_eq!(description.get("zh-CN").unwrap(), "中文描述");
        assert_eq!(description.get("es-ES").unwrap(), "Descripción en español");
    }

    #[test]
    fn test_manifest_without_description_field() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0"
        }"#;

        let manifest: Manifest = manifest_json.parse().unwrap();

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
                "en_US": "Should fail - using underscore instead of hyphen"
            }
        }"#;

        let result: Result<Manifest, _> = manifest_json.parse();
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {}", error_msg);
        assert!(
            error_msg.contains("Invalid locale format")
                || error_msg.contains("locale")
                || error_msg.contains("does not match")
                || error_msg.contains("en_US")
        );
    }

    #[test]
    fn test_manifest_with_empty_description() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "en-US": ""
            }
        }"#;

        let result: Result<Manifest, _> = manifest_json.parse();
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {}", error_msg);
        assert!(error_msg.contains("shorter than 1 character"));
    }

    #[test]
    fn test_manifest_with_simple_language_codes() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "en": "English description",
                "zh": "中文描述",
                "es": "Descripción en español"
            }
        }"#;

        let manifest: Manifest = manifest_json.parse().unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let description = manifest.description.unwrap();
        assert_eq!(description.get("en").unwrap(), "English description");
        assert_eq!(description.get("zh").unwrap(), "中文描述");
        assert_eq!(description.get("es").unwrap(), "Descripción en español");
    }

    #[test]
    fn test_manifest_with_mixed_locale_formats() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "en": "English description",
                "zh-CN": "简体中文描述",
                "zh-TW": "繁體中文描述",
                "es-ES": "Descripción en español"
            }
        }"#;

        let manifest: Manifest = manifest_json.parse().unwrap();

        let description = manifest.description.unwrap();
        assert_eq!(description.get("en").unwrap(), "English description");
        assert_eq!(description.get("zh-CN").unwrap(), "简体中文描述");
        assert_eq!(description.get("zh-TW").unwrap(), "繁體中文描述");
        assert_eq!(description.get("es-ES").unwrap(), "Descripción en español");
    }

    #[test]
    fn test_manifest_with_invalid_bcp47_formats() {
        // Test with uppercase language code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "EN-US": "Should fail - uppercase language code"
            }
        }"#;

        let result: Result<Manifest, _> = manifest_json.parse();
        assert!(result.is_err());

        // Test with lowercase region code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "en-us": "Should fail - lowercase region code"
            }
        }"#;

        let result: Result<Manifest, _> = manifest_json.parse();
        assert!(result.is_err());

        // Test with three-letter language code
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "eng-US": "Should fail - three-letter language code"
            }
        }"#;

        let result: Result<Manifest, _> = manifest_json.parse();
        assert!(result.is_err());
    }
}
