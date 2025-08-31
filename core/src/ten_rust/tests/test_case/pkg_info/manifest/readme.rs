//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

#[cfg(test)]
mod tests {
    use ten_rust::pkg_info::manifest::Manifest;

    #[tokio::test]
    async fn test_manifest_with_readme_content() {
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

        let manifest = Manifest::create_from_str(manifest_json).unwrap();

        assert!(manifest.readme.is_some());
        let readme = manifest.readme.unwrap();

        assert_eq!(readme.locales.len(), 2);
        assert!(readme.locales.contains_key("en-US"));
        assert!(readme.locales.contains_key("zh-CN"));

        let en_readme = &readme.locales["en-US"];
        assert_eq!(
            en_readme.content.as_ref().unwrap(),
            "This is a comprehensive README for the test extension."
        );
        assert!(en_readme.import_uri.is_none());

        let zh_readme = &readme.locales["zh-CN"];
        assert_eq!(zh_readme.content.as_ref().unwrap(), "这是测试扩展的完整说明文档。");
        assert!(zh_readme.import_uri.is_none());
    }

    #[tokio::test]
    async fn test_manifest_with_readme_import_uri() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {
                        "import_uri": "file://./docs/readme.md"
                    },
                    "zh-CN": {
                        "import_uri": "file://./docs/readme-zh.md"
                    }
                }
            }
        }"#;

        let manifest = Manifest::create_from_str(manifest_json).unwrap();

        assert!(manifest.readme.is_some());
        let readme = manifest.readme.unwrap();

        assert_eq!(readme.locales.len(), 2);

        let en_readme = &readme.locales["en-US"];
        assert_eq!(en_readme.import_uri.as_ref().unwrap(), "file://./docs/readme.md");
        assert!(en_readme.content.is_none());

        let zh_readme = &readme.locales["zh-CN"];
        assert_eq!(zh_readme.import_uri.as_ref().unwrap(), "file://./docs/readme-zh.md");
        assert!(zh_readme.content.is_none());
    }

    #[tokio::test]
    async fn test_manifest_without_readme() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0"
        }"#;

        let manifest = Manifest::create_from_str(manifest_json).unwrap();
        assert!(manifest.readme.is_none());
    }

    #[test]
    fn test_manifest_readme_invalid_locale() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "invalid-locale": {
                        "content": "Test content"
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Invalid locale format"));
    }

    #[test]
    fn test_manifest_readme_empty_content() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {
                        "content": ""
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Content for locale 'en-US' cannot be empty"));
    }

    #[test]
    fn test_manifest_readme_empty_import_uri() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {
                        "import_uri": ""
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Import URI for locale 'en-US' cannot be empty"));
    }

    #[test]
    fn test_manifest_readme_missing_content_and_import_uri() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {}
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Locale 'en-US' must have either 'content' or 'import_uri'"));
    }

    #[test]
    fn test_manifest_readme_both_content_and_import_uri() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "readme": {
                "locales": {
                    "en-US": {
                        "content": "Test content",
                        "import_uri": "file://./docs/readme.md"
                    }
                }
            }
        }"#;

        let result: Result<Manifest, _> = serde_json::from_str(manifest_json);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Locale 'en-US' cannot have both 'content' and 'import_uri'"));
    }
}
