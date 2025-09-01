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
    async fn test_pkg_registry_info_with_readme() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {
                        "content": "This is a comprehensive README for the test extension."
                    },
                    "zh-CN": {
                        "content": "这是测试扩展的完整说明文档。"
                    }
                }
            }
        }"#;

        let manifest: Manifest = Manifest::create_from_str(manifest_json).unwrap();
        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        assert!(pkg_registry_info.readme.is_some());
        let readme = pkg_registry_info.readme.unwrap();

        assert_eq!(
            readme.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "This is a comprehensive README for the test extension."
        );
        assert_eq!(
            readme.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "这是测试扩展的完整说明文档。"
        );
    }

    #[tokio::test]
    async fn test_pkg_registry_info_with_readme_import_uri() {
        // Create temporary directory for test files
        let temp_dir = tempfile::TempDir::new().unwrap();
        let temp_path = temp_dir.path();

        // Create test content files
        let docs_dir = temp_path.join("docs");
        std::fs::create_dir_all(&docs_dir).unwrap();

        let readme_en_path = docs_dir.join("readme-en.md");
        let readme_zh_path = docs_dir.join("readme-zh.md");

        std::fs::write(&readme_en_path, "English README content").unwrap();
        std::fs::write(&readme_zh_path, "Chinese README content").unwrap();

        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {
                        "import_uri": "docs/readme-en.md"
                    },
                    "zh-CN": {
                        "import_uri": "docs/readme-zh.md"
                    }
                }
            }
        }"#;

        let mut manifest: Manifest = Manifest::create_from_str(manifest_json).unwrap();

        // Set base_dir for readme locale contents
        let base_dir_str = temp_path.to_string_lossy().to_string();
        if let Some(ref mut readme) = manifest.readme {
            for (_locale, locale_content) in readme.locales.iter_mut() {
                locale_content.base_dir = Some(base_dir_str.clone());
            }
        }

        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        assert!(pkg_registry_info.readme.is_some());
        let readme = pkg_registry_info.readme.unwrap();

        // Verify content is resolved from import_uri
        assert!(readme.locales.get("en-US").unwrap().content.is_some());
        assert_eq!(
            readme.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "English README content"
        );

        assert!(readme.locales.get("zh-CN").unwrap().content.is_some());
        assert_eq!(
            readme.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "Chinese README content"
        );

        // Verify import_uri is still preserved
        assert!(readme.locales.get("en-US").unwrap().import_uri.is_some());
        assert_eq!(
            readme.locales.get("en-US").unwrap().import_uri.as_ref().unwrap(),
            "docs/readme-en.md"
        );
        assert!(readme.locales.get("zh-CN").unwrap().import_uri.is_some());
        assert_eq!(
            readme.locales.get("zh-CN").unwrap().import_uri.as_ref().unwrap(),
            "docs/readme-zh.md"
        );
    }

    #[tokio::test]
    async fn test_pkg_registry_info_without_readme() {
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

        assert!(pkg_registry_info.readme.is_none());
    }

    #[tokio::test]
    async fn test_pkg_registry_info_readme_conversion_to_pkg_info() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {
                        "content": "Test README content"
                    }
                }
            }
        }"#;

        let manifest: Manifest = Manifest::create_from_str(manifest_json).unwrap();
        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        // Convert back to PkgInfo
        let pkg_info: PkgInfo = (&pkg_registry_info).into();

        assert!(pkg_info.manifest.readme.is_some());
        let readme = pkg_info.manifest.readme.unwrap();
        assert_eq!(
            readme.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test README content"
        );
    }

    #[test]
    fn test_pkg_registry_info_serialization_with_readme() {
        let mut locales = HashMap::new();
        locales.insert(
            "en-US".to_string(),
            LocaleContent {
                content: Some("Test README".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );

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
            readme: Some(LocalizedField {
                locales,
            }),
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        assert!(serialized.contains("readme"));
        assert!(serialized.contains("Test README"));

        // Test deserialization
        let deserialized: PkgRegistryInfo = serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.readme.is_some());
        assert_eq!(
            deserialized.readme.unwrap().locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test README"
        );
    }

    #[test]
    fn test_pkg_registry_info_serialization_without_readme() {
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
        // Since readme is None and we use skip_serializing_if =
        // "Option::is_none", the readme field should not appear in
        // the serialized JSON
        assert!(!serialized.contains("readme"));

        // Test deserialization
        let deserialized: PkgRegistryInfo = serde_json::from_str(&serialized).unwrap();
        assert!(deserialized.readme.is_none());
    }

    #[test]
    fn test_pkg_registry_info_readme_mixed_content_and_import_uri() {
        let mut locales = HashMap::new();
        locales.insert(
            "en-US".to_string(),
            LocaleContent {
                content: Some("English README content".to_string()),
                import_uri: None,
                base_dir: Some(String::new()),
            },
        );
        locales.insert(
            "zh-CN".to_string(),
            LocaleContent {
                content: None,
                import_uri: Some("file://./docs/readme-zh.md".to_string()),
                base_dir: Some(String::new()),
            },
        );

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
            readme: Some(LocalizedField {
                locales,
            }),
        };

        let serialized = serde_json::to_string(&pkg_registry_info).unwrap();
        let deserialized: PkgRegistryInfo = serde_json::from_str(&serialized).unwrap();

        let readme = deserialized.readme.unwrap();
        assert_eq!(
            readme.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "English README content"
        );
        assert_eq!(
            readme.locales.get("zh-CN").unwrap().import_uri.as_ref().unwrap(),
            "file://./docs/readme-zh.md"
        );
    }
}
