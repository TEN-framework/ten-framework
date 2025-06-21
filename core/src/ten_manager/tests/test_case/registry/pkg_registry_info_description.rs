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

    #[test]
    fn test_pkg_registry_info_with_description() {
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
        let pkg_registry_info = get_pkg_registry_info_from_manifest(
            "https://example.com/test.tar.gz",
            &manifest,
        )
        .unwrap();

        assert!(pkg_registry_info.description.is_some());
        let description = pkg_registry_info.description.unwrap();

        assert_eq!(description.get("en-US").unwrap(), "English description");
        assert_eq!(description.get("zh-CN").unwrap(), "中文描述");
        assert_eq!(description.get("es-ES").unwrap(), "Descripción en español");
    }

    #[test]
    fn test_pkg_registry_info_without_description() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0"
        }"#;

        let manifest: Manifest = manifest_json.parse().unwrap();
        let pkg_registry_info = get_pkg_registry_info_from_manifest(
            "https://example.com/test.tar.gz",
            &manifest,
        )
        .unwrap();

        assert!(pkg_registry_info.description.is_none());
    }

    #[test]
    fn test_pkg_registry_info_serialization_with_description() {
        let mut description = HashMap::new();
        description.insert("en-US".to_string(), "Test description".to_string());
        description.insert("zh-CN".to_string(), "测试描述".to_string());

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
            description: Some(description),
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        assert!(serialized.contains("en-US"));
        assert!(serialized.contains("Test description"));
        assert!(serialized.contains("zh-CN"));
        assert!(serialized.contains("测试描述"));

        // Test deserialization
        let deserialized: PkgRegistryInfo =
            serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.description.is_some());
        let desc = deserialized.description.unwrap();
        assert_eq!(desc.get("en-US").unwrap(), "Test description");
        assert_eq!(desc.get("zh-CN").unwrap(), "测试描述");
    }

    #[test]
    fn test_pkg_registry_info_serialization_without_description() {
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
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        // Since description is None and we use skip_serializing_if =
        // "Option::is_none", the description field should not appear in
        // the serialized JSON
        assert!(!serialized.contains("description"));

        // Test deserialization
        let deserialized: PkgRegistryInfo =
            serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.description.is_none());
    }

    #[test]
    fn test_pkg_registry_info_to_pkg_info_conversion() {
        let mut description = HashMap::new();
        description.insert("en-US".to_string(), "Test description".to_string());
        description.insert("fr".to_string(), "Description de test".to_string());

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
            description: Some(description.clone()),
        };

        let pkg_info: PkgInfo = (&pkg_registry_info).into();

        assert!(pkg_info.manifest.description.is_some());
        let converted_description = pkg_info.manifest.description.unwrap();
        assert_eq!(converted_description, description);

        // Check that description is properly added to all_fields
        let all_fields = &pkg_info.manifest.all_fields;
        assert!(all_fields.contains_key("description"));

        let desc_value = &all_fields["description"];
        assert!(desc_value.is_object());
        let desc_obj = desc_value.as_object().unwrap();
        assert_eq!(
            desc_obj.get("en-US").unwrap().as_str().unwrap(),
            "Test description"
        );
        assert_eq!(
            desc_obj.get("fr").unwrap().as_str().unwrap(),
            "Description de test"
        );
    }
}
