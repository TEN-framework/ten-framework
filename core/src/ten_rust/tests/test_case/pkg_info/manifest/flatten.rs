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
    use ten_rust::pkg_info::manifest::Manifest;

    #[tokio::test]
    async fn test_manifest_flatten_import_uri() {
        // Create a temporary directory for testing
        let temp_dir = TempDir::new().unwrap();
        let temp_path = temp_dir.path();

        // Create test files with content
        let readme_content = "This is a test README content.";
        let description_content = "This is a test description content.";
        let display_name_content = "Test Display Name";

        fs::write(temp_path.join("readme.md"), readme_content).unwrap();
        fs::write(temp_path.join("description.txt"), description_content)
            .unwrap();
        fs::write(temp_path.join("display_name.txt"), display_name_content)
            .unwrap();

        // Create a manifest JSON with import_uri fields
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en": {
                        "import_uri": "readme.md"
                    }
                }
            },
            "description": {
                "locales": {
                    "en": {
                        "import_uri": "description.txt"
                    }
                }
            },
            "display_name": {
                "locales": {
                    "en": {
                        "import_uri": "display_name.txt"
                    }
                }
            }
        }"#;

        // Parse the manifest
        let mut manifest =
            Manifest::create_from_str(manifest_json).await.unwrap();

        // Before flattening, content should be None and import_uri should be
        // Some
        assert!(manifest.readme.as_ref().unwrap().locales["en"]
            .content
            .is_none());
        assert!(manifest.readme.as_ref().unwrap().locales["en"]
            .import_uri
            .is_some());
        assert!(manifest.description.as_ref().unwrap().locales["en"]
            .content
            .is_none());
        assert!(manifest.description.as_ref().unwrap().locales["en"]
            .import_uri
            .is_some());
        assert!(manifest.display_name.as_ref().unwrap().locales["en"]
            .content
            .is_none());
        assert!(manifest.display_name.as_ref().unwrap().locales["en"]
            .import_uri
            .is_some());

        // Flatten the manifest
        let base_dir = temp_path.to_str().unwrap();
        Manifest::flatten(&mut manifest, base_dir).await.unwrap();

        // After flattening, content should be populated with file contents
        assert_eq!(
            manifest.readme.as_ref().unwrap().locales["en"]
                .content
                .as_ref()
                .unwrap(),
            readme_content
        );
        assert_eq!(
            manifest.description.as_ref().unwrap().locales["en"]
                .content
                .as_ref()
                .unwrap(),
            description_content
        );
        assert_eq!(
            manifest.display_name.as_ref().unwrap().locales["en"]
                .content
                .as_ref()
                .unwrap(),
            display_name_content
        );

        // import_uri should still be present
        assert!(manifest.readme.as_ref().unwrap().locales["en"]
            .import_uri
            .is_some());
        assert!(manifest.description.as_ref().unwrap().locales["en"]
            .import_uri
            .is_some());
        assert!(manifest.display_name.as_ref().unwrap().locales["en"]
            .import_uri
            .is_some());
    }

    #[tokio::test]
    async fn test_manifest_flatten_with_existing_content() {
        // Create a temporary directory for testing
        let temp_dir = TempDir::new().unwrap();
        let temp_path = temp_dir.path();

        // Create a test file
        let readme_file_content = "This content should not be used.";
        fs::write(temp_path.join("readme.md"), readme_file_content).unwrap();

        // Create a manifest JSON with both content and import_uri (content
        // should take precedence)
        let existing_content = "Existing README content";
        let manifest_json = format!(
            r#"{{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {{
                "locales": {{
                    "en": {{
                        "content": "{existing_content}"
                    }}
                }}
            }}
        }}"#
        );

        // Parse the manifest
        let mut manifest =
            Manifest::create_from_str(&manifest_json).await.unwrap();

        // Before flattening, content should be the existing content
        assert_eq!(
            manifest.readme.as_ref().unwrap().locales["en"]
                .content
                .as_ref()
                .unwrap(),
            existing_content
        );

        // Flatten the manifest
        let base_dir = temp_path.to_str().unwrap();
        Manifest::flatten(&mut manifest, base_dir).await.unwrap();

        // After flattening, content should remain unchanged since it already
        // had content
        assert_eq!(
            manifest.readme.as_ref().unwrap().locales["en"]
                .content
                .as_ref()
                .unwrap(),
            existing_content
        );
    }

    #[tokio::test]
    async fn test_manifest_flatten_missing_file() {
        // Create a manifest JSON with import_uri pointing to non-existent file
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en": {
                        "import_uri": "non_existent_file.md"
                    }
                }
            }
        }"#;

        // Parse the manifest
        let mut manifest =
            Manifest::create_from_str(manifest_json).await.unwrap();

        // Create a temporary directory for testing
        let temp_dir = TempDir::new().unwrap();
        let temp_path = temp_dir.path();

        // Flatten the manifest should fail
        let base_dir = temp_path.to_str().unwrap();
        let result = Manifest::flatten(&mut manifest, base_dir).await;
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Failed to read content from import_uri"));
    }
}
