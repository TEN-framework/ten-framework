//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod description;
mod display_name;
mod interface;
mod readme;

#[cfg(test)]
mod tests {
    use anyhow::Result;
    use ten_rust::pkg_info::{manifest::Manifest, pkg_type::PkgType};

    #[tokio::test]
    async fn test_extension_manifest_from_str() {
        let manifest_str = include_str!("../../../test_data/test_extension_manifest.json");

        let result: Result<Manifest> = Manifest::create_from_str(manifest_str);
        assert!(result.is_ok());

        let manifest = result.unwrap();
        assert_eq!(manifest.type_and_name.pkg_type, PkgType::Extension);

        let cmd_in = manifest.api.unwrap().cmd_in.unwrap();
        assert_eq!(cmd_in.len(), 1);

        let property = cmd_in[0].property.as_ref().unwrap();
        let required = property.required.as_ref();
        assert!(required.is_some());
        assert_eq!(required.unwrap().len(), 1);
    }

    #[tokio::test]
    async fn test_manifest_duplicate_dependencies_should_fail() {
        let manifest_str = r#"
        {
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "dependencies": [
                {
                    "type": "extension",
                    "name": "duplicate_ext",
                    "version": "^1.0.0"
                },
                {
                    "type": "extension",
                    "name": "duplicate_ext",
                    "version": "^2.0.0"
                }
            ]
        }"#;

        let result: Result<Manifest> = Manifest::create_from_str(manifest_str);
        assert!(result.is_err());

        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg.contains("Duplicate dependency found"));
        assert!(error_msg.contains("extension"));
        assert!(error_msg.contains("duplicate_ext"));
    }

    #[tokio::test]
    async fn test_manifest_different_type_same_name_should_pass() {
        let manifest_str = r#"
        {
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "dependencies": [
                {
                    "type": "extension",
                    "name": "same_name",
                    "version": "^1.0.0"
                },
                {
                    "type": "protocol",
                    "name": "same_name",
                    "version": "^1.0.0"
                }
            ]
        }"#;

        let result: Result<Manifest> = Manifest::create_from_str(manifest_str);
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_manifest_local_dependencies_should_not_conflict() {
        let manifest_str = r#"
        {
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "dependencies": [
                {
                    "path": "../path1"
                },
                {
                    "path": "../path2"
                },
                {
                    "type": "extension",
                    "name": "registry_ext",
                    "version": "^1.0.0"
                }
            ]
        }"#;

        let result: Result<Manifest> = Manifest::create_from_str(manifest_str);
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_bytedance_tts_manifest_from_str() {
        let manifest_str = r#"
        {
          "type": "extension",
          "name": "bytedance_tts",
          "version": "0.1.0",
          "dependencies": [
            {
              "type": "system",
              "name": "ten_runtime_python",
              "version": "0.10"
            }
          ],
          "package": {
            "include": [
              "manifest.json",
              "property.json",
              "BUILD.gn",
              "**.tent",
              "**.py",
              "README.md",
              "tests/**"
            ]
          },
          "api": {
            "property": {
              "properties": {
                "appid": {
                  "type": "string"
                },
                "token": {
                  "type": "string"
                },
                "voice_type": {
                  "type": "string"
                },
                "sample_rate": {
                  "type": "int64"
                },
                "api_url": {
                  "type": "string"
                },
                "cluster": {
                  "type": "string"
                }
              }
            },
            "cmd_in": [
              {
                "name": "flush"
              }
            ],
            "cmd_out": [
              {
                "name": "flush"
              }
            ],
            "data_in": [
              {
                "name": "text_data",
                "property": {
                  "properties": {
                    "text": {
                      "type": "string"
                    }
                  }
                }
              }
            ],
            "audio_frame_out": [
              {
                "name": "pcm_frame"
              }
            ]
          }
        }"#;

        let result: Result<Manifest> = Manifest::create_from_str(manifest_str);
        assert!(result.is_ok());

        let manifest = result.unwrap();
        assert_eq!(manifest.type_and_name.pkg_type, PkgType::Extension);
        assert_eq!(manifest.type_and_name.name, "bytedance_tts");
        assert_eq!(manifest.version.to_string(), "0.1.0");

        // Test dependencies
        let dependencies = manifest.dependencies.as_ref().unwrap();
        assert_eq!(dependencies.len(), 1);

        let dep_type_and_name = dependencies[0].get_type_and_name().await;
        assert!(dep_type_and_name.is_some());
        let (dep_type, dep_name) = dep_type_and_name.unwrap();
        assert_eq!(dep_type, PkgType::System);
        assert_eq!(dep_name, "ten_runtime_python");

        // Test API
        let api = manifest.api.as_ref().unwrap();

        // Test cmd_in
        let cmd_in = api.cmd_in.as_ref().unwrap();
        assert_eq!(cmd_in.len(), 1);
        assert_eq!(cmd_in[0].name, "flush");

        // Test cmd_out
        let cmd_out = api.cmd_out.as_ref().unwrap();
        assert_eq!(cmd_out.len(), 1);
        assert_eq!(cmd_out[0].name, "flush");

        // Test data_in
        let data_in = api.data_in.as_ref().unwrap();
        assert_eq!(data_in.len(), 1);
        assert_eq!(data_in[0].name, "text_data");

        // Test audio_frame_out
        let audio_frame_out = api.audio_frame_out.as_ref().unwrap();
        assert_eq!(audio_frame_out.len(), 1);
        assert_eq!(audio_frame_out[0].name, "pcm_frame");
    }

    #[test]
    fn test_api_property_description_string_and_localized() {
        let manifest_json = r#"{
            "type": "extension",
            "name": "test_extension",
            "version": "1.0.0",
            "description": {
                "locales": {
                    "en-US": {
                        "content": "Test extension"
                    }
                }
            },
            "dependencies": [],
            "api": {
                "property": {
                    "properties": {
                        "api_key": {
                            "type": "string",
                            "description": "Simple string description for API key"
                        },
                        "config": {
                            "type": "object",
                            "description": {
                                "locales": {
                                    "en-US": {
                                        "content": "Configuration object"
                                    },
                                    "zh-CN": {
                                        "content": "配置对象"
                                    }
                                }
                            },
                            "properties": {
                                "timeout": {
                                    "type": "int32",
                                    "description": "Timeout value in milliseconds"
                                },
                                "retry": {
                                    "type": "int32",
                                    "description": {
                                        "locales": {
                                            "en-US": {
                                                "content": "Number of retries"
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }"#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();

        let api = manifest.api.as_ref().unwrap();
        let properties = api.property.as_ref().unwrap().properties.as_ref().unwrap();

        // Test simple string description
        let api_key = properties.get("api_key").unwrap();
        assert!(api_key.description.is_some());
        let api_key_desc = api_key.description.as_ref().unwrap();
        assert_eq!(
            api_key_desc.as_string().unwrap(),
            "Simple string description for API key"
        );

        // Test localized description
        let config = properties.get("config").unwrap();
        assert!(config.description.is_some());
        let config_desc = config.description.as_ref().unwrap();
        let config_localized = config_desc.as_localized().unwrap();
        assert_eq!(
            config_localized.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Configuration object"
        );
        assert_eq!(
            config_localized.locales.get("zh-CN").unwrap().content.as_ref().unwrap(),
            "配置对象"
        );

        // Test nested properties
        let config_props = config.properties.as_ref().unwrap();

        let timeout = config_props.get("timeout").unwrap();
        assert!(timeout.description.is_some());
        assert_eq!(
            timeout.description.as_ref().unwrap().as_string().unwrap(),
            "Timeout value in milliseconds"
        );

        let retry = config_props.get("retry").unwrap();
        assert!(retry.description.is_some());
        let retry_localized = retry.description.as_ref().unwrap().as_localized().unwrap();
        assert_eq!(
            retry_localized.locales.get("en-US").unwrap().content.as_ref().unwrap(),
            "Number of retries"
        );

        // Test serialization roundtrip
        let serialized = serde_json::to_string(&manifest).unwrap();
        let deserialized: Manifest = serde_json::from_str(&serialized).unwrap();

        let des_api = deserialized.api.as_ref().unwrap();
        let des_properties = des_api.property.as_ref().unwrap().properties.as_ref().unwrap();
        let des_api_key = des_properties.get("api_key").unwrap();
        assert_eq!(
            des_api_key.description.as_ref().unwrap().as_string().unwrap(),
            "Simple string description for API key"
        );
    }
}
