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
}
