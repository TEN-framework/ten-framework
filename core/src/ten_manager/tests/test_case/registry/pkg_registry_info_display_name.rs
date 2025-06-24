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
    use ten_rust::pkg_info::manifest::Manifest;
    use ten_rust::pkg_info::pkg_basic_info::PkgBasicInfo;
    use ten_rust::pkg_info::PkgInfo;

    #[tokio::test]
    async fn test_pkg_registry_info_with_display_name() {
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

        let manifest: Manifest =
            Manifest::create_from_str(manifest_json).await.unwrap();
        let pkg_registry_info = get_pkg_registry_info_from_manifest(
            "https://example.com/test.tar.gz",
            &manifest,
        )
        .await
        .unwrap();

        assert!(pkg_registry_info.display_name.is_some());
        let display_name = pkg_registry_info.display_name.unwrap();

        assert_eq!(display_name.get("en-US").unwrap(), "Test Extension");
        assert_eq!(display_name.get("zh-CN").unwrap(), "测试扩展");
        assert_eq!(display_name.get("es-ES").unwrap(), "Extensión de Prueba");
    }

    #[tokio::test]
    async fn test_pkg_registry_info_without_display_name() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0"
        }"#;

        let manifest: Manifest =
            Manifest::create_from_str(manifest_json).await.unwrap();
        let pkg_registry_info = get_pkg_registry_info_from_manifest(
            "https://example.com/test.tar.gz",
            &manifest,
        )
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
                "en-US": "This is a test extension for demonstration purposes",
                "zh-CN": "这是用于演示目的的测试扩展"
            },
            "display_name": {
                "en-US": "Test Extension",
                "zh-CN": "测试扩展"
            }
        }"#;

        let manifest: Manifest =
            Manifest::create_from_str(manifest_json).await.unwrap();
        let pkg_registry_info = get_pkg_registry_info_from_manifest(
            "https://example.com/test.tar.gz",
            &manifest,
        )
        .await
        .unwrap();

        assert!(pkg_registry_info.description.is_some());
        assert!(pkg_registry_info.display_name.is_some());

        let description = pkg_registry_info.description.unwrap();
        let display_name = pkg_registry_info.display_name.unwrap();

        assert_eq!(
            description.get("en-US").unwrap(),
            "This is a test extension for demonstration purposes"
        );
        assert_eq!(
            description.get("zh-CN").unwrap(),
            "这是用于演示目的的测试扩展"
        );

        assert_eq!(display_name.get("en-US").unwrap(), "Test Extension");
        assert_eq!(display_name.get("zh-CN").unwrap(), "测试扩展");
    }

    #[test]
    fn test_pkg_registry_info_serialization_with_display_name() {
        let mut display_name = HashMap::new();
        display_name.insert("en-US".to_string(), "Test Extension".to_string());
        display_name.insert("zh-CN".to_string(), "测试扩展".to_string());

        let pkg_registry_info = PkgRegistryInfo {
            basic_info: PkgBasicInfo {
                type_and_name:
                    ten_rust::pkg_info::pkg_type_and_name::PkgTypeAndName {
                        pkg_type:
                            ten_rust::pkg_info::pkg_type::PkgType::Extension,
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
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        assert!(serialized.contains("en-US"));
        assert!(serialized.contains("Test Extension"));
        assert!(serialized.contains("zh-CN"));
        assert!(serialized.contains("测试扩展"));
        assert!(serialized.contains("display_name"));

        // Test deserialization
        let deserialized: PkgRegistryInfo =
            serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.display_name.is_some());
        let display_name = deserialized.display_name.unwrap();
        assert_eq!(display_name.get("en-US").unwrap(), "Test Extension");
        assert_eq!(display_name.get("zh-CN").unwrap(), "测试扩展");
    }

    #[test]
    fn test_pkg_registry_info_serialization_without_display_name() {
        let pkg_registry_info = PkgRegistryInfo {
            basic_info: PkgBasicInfo {
                type_and_name:
                    ten_rust::pkg_info::pkg_type_and_name::PkgTypeAndName {
                        pkg_type:
                            ten_rust::pkg_info::pkg_type::PkgType::Extension,
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
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        // Since display_name is None and we use skip_serializing_if =
        // "Option::is_none", the display_name field should not appear in
        // the serialized JSON
        assert!(!serialized.contains("display_name"));

        // Test deserialization
        let deserialized: PkgRegistryInfo =
            serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.display_name.is_none());
    }

    #[test]
    fn test_pkg_registry_info_to_pkg_info_conversion() {
        let mut display_name = HashMap::new();
        display_name.insert("en-US".to_string(), "Test Extension".to_string());
        display_name.insert("fr".to_string(), "Extension de Test".to_string());

        let pkg_registry_info = PkgRegistryInfo {
            basic_info: PkgBasicInfo {
                type_and_name:
                    ten_rust::pkg_info::pkg_type_and_name::PkgTypeAndName {
                        pkg_type:
                            ten_rust::pkg_info::pkg_type::PkgType::Extension,
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
        };

        let pkg_info: PkgInfo = (&pkg_registry_info).into();

        assert!(pkg_info.manifest.display_name.is_some());
        let converted_display_name = pkg_info.manifest.display_name.unwrap();
        assert_eq!(converted_display_name, display_name);

        // Check that display_name is properly added to all_fields
        let all_fields = &pkg_info.manifest.all_fields;
        assert!(all_fields.contains_key("display_name"));

        let display_name_value = &all_fields["display_name"];
        assert!(display_name_value.is_object());
        let display_name_obj = display_name_value.as_object().unwrap();
        assert_eq!(
            display_name_obj.get("en-US").unwrap().as_str().unwrap(),
            "Test Extension"
        );
        assert_eq!(
            display_name_obj.get("fr").unwrap().as_str().unwrap(),
            "Extension de Test"
        );
    }
}
