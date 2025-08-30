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
    use ten_rust::pkg_info::{manifest::Manifest, pkg_basic_info::PkgBasicInfo, PkgInfo};

    #[tokio::test]
    async fn test_pkg_registry_info_with_display_name() {
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

        let manifest: Manifest = Manifest::create_from_str(manifest_json).unwrap();
        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        assert!(pkg_registry_info.display_name.is_some());
        let display_name = pkg_registry_info.display_name.unwrap();

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

    #[tokio::test]
    async fn test_pkg_registry_info_without_display_name() {
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

        assert!(pkg_registry_info.display_name.is_none());
    }

    #[tokio::test]
    async fn test_pkg_registry_info_with_both_description_and_display_name() {
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

        let manifest: Manifest = Manifest::create_from_str(manifest_json).unwrap();
        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        assert!(pkg_registry_info.description.is_some());
        assert!(pkg_registry_info.display_name.is_some());

        let description = pkg_registry_info.description.unwrap();
        let display_name = pkg_registry_info.display_name.unwrap();

        assert_eq!(
            description.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "This is a test extension for demonstration purposes"
        );
        assert_eq!(
            description.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "这是用于演示目的的测试扩展"
        );

        assert_eq!(
            display_name.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );
        assert_eq!(
            display_name.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "测试扩展"
        );
    }

    #[test]
    fn test_pkg_registry_info_serialization_with_display_name() {
        let mut locales = HashMap::new();
        locales.insert(
            "en-US".to_string(),
            ten_rust::pkg_info::manifest::LocaleContent {
                content: Some("Test Extension".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );
        locales.insert(
            "zh-CN".to_string(),
            ten_rust::pkg_info::manifest::LocaleContent {
                content: Some("测试扩展".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );

        let display_name = ten_rust::pkg_info::manifest::LocalizedField {
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
            description: None,
            display_name: Some(display_name),
            readme: None,
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        assert!(serialized.contains("en-US"));
        assert!(serialized.contains("Test Extension"));
        assert!(serialized.contains("zh-CN"));
        assert!(serialized.contains("测试扩展"));
        assert!(serialized.contains("display_name"));

        // Test deserialization
        let deserialized: PkgRegistryInfo = serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.display_name.is_some());
        let display_name = deserialized.display_name.unwrap();
        assert_eq!(
            display_name.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );
        assert_eq!(
            display_name.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "测试扩展"
        );
    }

    #[test]
    fn test_pkg_registry_info_serialization_without_display_name() {
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
        // Since display_name is None and we use skip_serializing_if =
        // "Option::is_none", the display_name field should not appear in
        // the serialized JSON
        assert!(!serialized.contains("display_name"));

        // Test deserialization
        let deserialized: PkgRegistryInfo = serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.display_name.is_none());
    }

    #[test]
    fn test_pkg_registry_info_to_pkg_info_conversion() {
        let mut locales = HashMap::new();
        locales.insert(
            "en-US".to_string(),
            ten_rust::pkg_info::manifest::LocaleContent {
                content: Some("Test Extension".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );
        locales.insert(
            "fr".to_string(),
            ten_rust::pkg_info::manifest::LocaleContent {
                content: Some("Extension de Test".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );

        let display_name = ten_rust::pkg_info::manifest::LocalizedField {
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
            description: None,
            display_name: Some(display_name.clone()),
            readme: None,
        };

        let pkg_info: PkgInfo = (&pkg_registry_info).into();

        assert!(pkg_info.manifest.display_name.is_some());
        let converted_display_name = pkg_info.manifest.display_name.unwrap();
        assert_eq!(
            converted_display_name.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );
        assert_eq!(
            converted_display_name.locales.get("fr").unwrap().content.as_ref().unwrap(),
            "Extension de Test"
        );
    }
}
