//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::fs;

    use tempfile::TempDir;
    use ten_manager::registry::found_result::get_pkg_registry_info_from_manifest;
    use ten_rust::pkg_info::manifest::Manifest;

    /// Test that verifies get_packages_endpoint correctly resolves import_uri
    /// fields to actual content when handling manifest with import_uri fields.
    ///
    /// This test verifies that import_uri fields are resolved to their actual
    /// content when creating PkgRegistryInfo.
    #[tokio::test]
    async fn test_get_pkg_registry_info_from_manifest_with_import_uri_fields() {
        // Create temporary directory for test files
        let temp_dir = TempDir::new().unwrap();
        let temp_path = temp_dir.path();

        // Create test content files
        let display_name_en_path = temp_path.join("display_name_en.txt");
        let description_en_path = temp_path.join("description_en.txt");
        let readme_en_path = temp_path.join("readme_en.md");

        fs::write(&display_name_en_path, "Test Extension").unwrap();
        fs::write(&description_en_path, "This is a test extension for demonstration").unwrap();
        fs::write(&readme_en_path, "# Test Extension\n\nThis is a comprehensive test extension.")
            .unwrap();

        // Create manifest with import_uri fields using relative paths
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension_with_import_uri",
            "version": "1.0.0",
            "display_name": {
                "locales": {
                    "en-US": {
                        "import_uri": "display_name_en.txt"
                    },
                    "zh-CN": {
                        "content": "测试扩展"
                    }
                }
            },
            "description": {
                "locales": {
                    "en-US": {
                        "import_uri": "description_en.txt"
                    }
                }
            },
            "readme": {
                "locales": {
                    "en-US": {
                        "import_uri": "readme_en.md"
                    }
                }
            }
        }"#;

        // Parse manifest and set base_dir for locale contents
        let mut manifest = Manifest::create_from_str(manifest_json).unwrap();

        let base_dir_str = temp_path.to_string_lossy().to_string();

        // Set base_dir for display_name
        if let Some(ref mut display_name) = manifest.display_name {
            for (_locale, locale_content) in display_name.locales.iter_mut() {
                locale_content.base_dir = Some(base_dir_str.clone());
            }
        }

        // Set base_dir for description
        if let Some(ref mut description) = manifest.description {
            for (_locale, locale_content) in description.locales.iter_mut() {
                locale_content.base_dir = Some(base_dir_str.clone());
            }
        }

        // Set base_dir for readme
        if let Some(ref mut readme) = manifest.readme {
            for (_locale, locale_content) in readme.locales.iter_mut() {
                locale_content.base_dir = Some(base_dir_str.clone());
            }
        }

        // Create pkg_registry_info
        let pkg_registry_info =
            get_pkg_registry_info_from_manifest("https://example.com/test.tar.gz", &manifest)
                .await
                .unwrap();

        // Verify that import_uri fields are resolved to actual content
        assert!(pkg_registry_info.display_name.is_some());
        assert!(pkg_registry_info.description.is_some());
        assert!(pkg_registry_info.readme.is_some());

        let display_name = pkg_registry_info.display_name.unwrap();
        let description = pkg_registry_info.description.unwrap();
        let readme = pkg_registry_info.readme.unwrap();

        // Verify content is now available (resolved from import_uri)
        assert!(display_name.locales.get("en-US").unwrap().content.is_some());
        assert_eq!(
            display_name.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Test Extension"
        );

        assert!(description.locales.get("en-US").unwrap().content.is_some());
        assert_eq!(
            description.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "This is a test extension for demonstration"
        );

        assert!(readme.locales.get("en-US").unwrap().content.is_some());
        assert_eq!(
            readme.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "# Test Extension\n\nThis is a comprehensive test extension."
        );

        // Verify mixed content (zh-CN has direct content)
        assert!(display_name.locales.get("zh-CN").unwrap().content.is_some());
        assert_eq!(
            display_name.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "测试扩展"
        );

        // Verify import_uri is still preserved (not cleared)
        assert!(display_name.locales.get("en-US").unwrap().import_uri.is_some());
        assert!(description.locales.get("en-US").unwrap().import_uri.is_some());
        assert!(readme.locales.get("en-US").unwrap().import_uri.is_some());

        println!("✅ import_uri fields are resolved to actual content while preserving import_uri");
        println!("📝 Note: get_packages_endpoint now resolves import_uri to actual content");
    }

    /// Test using LocaleContent.get_content() method to demonstrate
    /// how import_uri should be resolved to actual content.
    #[tokio::test]
    async fn test_locale_content_get_content_with_import_uri() {
        // Create temporary directory for test files
        let temp_dir = TempDir::new().unwrap();
        let temp_path = temp_dir.path();

        // Create test content file
        let content_file_path = temp_path.join("test_content.txt");
        let expected_content = "This is test content loaded from file";
        fs::write(&content_file_path, expected_content).unwrap();

        // Create manifest with import_uri and set base_dir
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "display_name": {
                "locales": {
                    "en-US": {
                        "import_uri": "test_content.txt"
                    }
                }
            }
        }"#;

        let mut manifest = Manifest::create_from_str(manifest_json).unwrap();

        // Set base_dir for the locale content (simulating
        // parse_manifest_from_file behavior)
        if let Some(ref mut display_name) = manifest.display_name {
            for (_locale, locale_content) in display_name.locales.iter_mut() {
                locale_content.base_dir = Some(temp_path.to_string_lossy().to_string());
            }
        }

        // Test that get_content() can resolve import_uri
        let display_name = manifest.display_name.unwrap();
        let en_locale_content = &display_name.locales["en-US"];

        let actual_content = en_locale_content.get_content().await.unwrap();
        assert_eq!(actual_content, expected_content);

        println!("✅ LocaleContent.get_content() correctly resolves import_uri to actual content");
    }
}
