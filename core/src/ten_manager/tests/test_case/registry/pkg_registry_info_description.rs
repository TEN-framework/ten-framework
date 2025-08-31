//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use ten_manager::registry::found_result::{
        get_pkg_registry_info_from_manifest, PkgRegistryInfo,
    };
    use ten_rust::pkg_info::{
        manifest::{LocaleContent, LocalizedField, Manifest},
        pkg_basic_info::PkgBasicInfo,
        PkgInfo,
    };

    #[tokio::test]
    async fn test_pkg_registry_info_with_description() {
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

        let manifest: Manifest = Manifest::create_from_str(manifest_json).unwrap();
        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        assert!(pkg_registry_info.description.is_some());
        let description = pkg_registry_info.description.unwrap();

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

    #[tokio::test]
    async fn test_pkg_registry_info_without_description() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0"
        }"#;

        let manifest: Manifest = Manifest::create_from_str(manifest_json).unwrap();
        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        assert!(pkg_registry_info.description.is_none());
    }

    #[test]
    fn test_pkg_registry_info_serialization_with_description() {
        let mut locales = HashMap::new();
        locales.insert(
            "en-US".to_string(),
            LocaleContent {
                content: Some("Test description".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );
        locales.insert(
            "zh-CN".to_string(),
            LocaleContent {
                content: Some("测试描述".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );

        let description = LocalizedField {
            locales,
        };

        let pkg_registry_info = PkgRegistryInfo {
            basic_info: PkgBasicInfo {
                type_and_name: ten_rust::pkg_info::pkg_type_and_name::PkgTypeAndName {
                    pkg_type: ten_rust::pkg_info::pkg_type::PkgType::Extension,
                    name: "test_extension".to_string(),
                },
                version: semver::Version::parse("1.0.0").unwrap(),
                supports: vec![],
            },
            dependencies: vec![],
            hash: "test_hash".to_string(),
            download_url: "https://example.com/test.tar.gz".to_string(),
            content_format: None,
            tags: None,
            description: Some(description),
            display_name: None,
            readme: None,
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        assert!(serialized.contains("en-US"));
        assert!(serialized.contains("Test description"));
        assert!(serialized.contains("zh-CN"));
        assert!(serialized.contains("测试描述"));

        // Test deserialization
        let deserialized: PkgRegistryInfo = serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.description.is_some());
        let desc = deserialized.description.unwrap();
        assert_eq!(
            desc.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test description"
        );
        assert_eq!(desc.locales.get("zh-CN").unwrap().content.as_ref().unwrap(), "测试描述");
    }

    #[test]
    fn test_pkg_registry_info_serialization_without_description() {
        let pkg_registry_info = PkgRegistryInfo {
            basic_info: PkgBasicInfo {
                type_and_name: ten_rust::pkg_info::pkg_type_and_name::PkgTypeAndName {
                    pkg_type: ten_rust::pkg_info::pkg_type::PkgType::Extension,
                    name: "test_extension".to_string(),
                },
                version: semver::Version::parse("1.0.0").unwrap(),
                supports: vec![],
            },
            dependencies: vec![],
            hash: "test_hash".to_string(),
            download_url: "https://example.com/test.tar.gz".to_string(),
            content_format: None,
            tags: None,
            description: None,
            display_name: None,
            readme: None,
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        // Since description is None and we use skip_serializing_if =
        // "Option::is_none", the description field should not appear in
        // the serialized JSON
        assert!(!serialized.contains("description"));

        // Test deserialization
        let deserialized: PkgRegistryInfo = serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.description.is_none());
    }

    #[test]
    fn test_pkg_registry_info_to_pkg_info_conversion() {
        let mut locales = HashMap::new();
        locales.insert(
            "en-US".to_string(),
            LocaleContent {
                content: Some("Test description".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );
        locales.insert(
            "fr".to_string(),
            LocaleContent {
                content: Some("Description de test".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );

        let description = LocalizedField {
            locales,
        };

        let pkg_registry_info = PkgRegistryInfo {
            basic_info: PkgBasicInfo {
                type_and_name: ten_rust::pkg_info::pkg_type_and_name::PkgTypeAndName {
                    pkg_type: ten_rust::pkg_info::pkg_type::PkgType::Extension,
                    name: "test_extension".to_string(),
                },
                version: semver::Version::parse("1.0.0").unwrap(),
                supports: vec![],
            },
            dependencies: vec![],
            hash: "test_hash".to_string(),
            download_url: "https://example.com/test.tar.gz".to_string(),
            content_format: None,
            tags: None,
            description: Some(description.clone()),
            display_name: None,
            readme: None,
        };

        let pkg_info: PkgInfo = (&pkg_registry_info).into();

        assert!(pkg_info.manifest.description.is_some());
        let converted_description = pkg_info.manifest.description.unwrap();
        assert_eq!(
            converted_description.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test description"
        );
        assert_eq!(
            converted_description.locales.get("fr").unwrap().content.as_ref().unwrap(),
            "Description de test"
        );
    }
}
