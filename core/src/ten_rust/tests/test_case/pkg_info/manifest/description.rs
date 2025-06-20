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
                "en": "English description",
                "cn": "中文描述",
                "es": "Descripción en español"
            }
        }"#;

        let manifest: Manifest = manifest_json.parse().unwrap();

        assert_eq!(manifest.type_and_name.name, "test_extension");
        assert_eq!(manifest.version.to_string(), "1.0.0");

        let description = manifest.description.unwrap();
        assert_eq!(description.get("en").unwrap(), "English description");
        assert_eq!(description.get("cn").unwrap(), "中文描述");
        assert_eq!(description.get("es").unwrap(), "Descripción en español");
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
                "invalid-locale": "Should fail"
            }
        }"#;

        let result: Result<Manifest, _> = manifest_json.parse();
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {}", error_msg);
        assert!(
            error_msg.contains("Invalid locale format")
                || error_msg.contains("locale")
        );
    }

    #[test]
    fn test_manifest_with_empty_description() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "en": ""
            }
        }"#;

        let result: Result<Manifest, _> = manifest_json.parse();
        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        println!("Actual error: {}", error_msg);
        assert!(error_msg.contains("shorter than 1 character"));
    }
}
