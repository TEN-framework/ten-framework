//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::path::Path;

    use ten_rust::pkg_info::{
        manifest::{parse_manifest_from_file, Manifest},
        value_type::ValueType,
    };

    #[tokio::test]
    async fn test_extension_manifest_with_interface_from_str() {
        let manifest_file_path =
            Path::new("tests/test_data/extension_manifest_with_interface.json");

        let manifest = parse_manifest_from_file(manifest_file_path).await.unwrap();

        let api = manifest.api.as_ref().unwrap();
        assert!(api.interface.is_some());
        assert_eq!(api.interface.as_ref().unwrap().len(), 1);

        let interface = api.interface.as_ref().unwrap()[0].clone();
        assert_eq!(interface.import_uri, "./interface/foo_1.json");

        // Check the flattened manifest api.
        let flattened_manifest = manifest.get_flattened_api(None).await.unwrap();
        let flattened_manifest = flattened_manifest.as_ref().unwrap();
        assert!(flattened_manifest.interface.is_none());

        // Check the properties.
        let property = flattened_manifest.property.as_ref().unwrap();
        let properties = property.properties.as_ref().unwrap();
        assert_eq!(properties.len(), 3);
        assert_eq!(properties.get("foo").unwrap().prop_type, ValueType::Bool);
        assert_eq!(properties.get("bar").unwrap().prop_type, ValueType::Int64);
        assert_eq!(properties.get("key").unwrap().prop_type, ValueType::String);

        // Check the cmd_in/cmd_out/data_in/data_out/audio_frame_in/
        // audio_frame_out/video_frame_in/video_frame_out.
        assert_eq!(flattened_manifest.cmd_in.as_ref().unwrap().len(), 3);
        assert_eq!(flattened_manifest.cmd_out.as_ref().unwrap().len(), 2);
        assert_eq!(flattened_manifest.data_in.as_ref().unwrap().len(), 1);
        assert!(flattened_manifest.data_out.is_none());
        assert!(flattened_manifest.audio_frame_in.is_none());
        assert!(flattened_manifest.audio_frame_out.is_none());
        assert!(flattened_manifest.video_frame_in.is_none());
        assert!(flattened_manifest.video_frame_out.is_none());
    }

    #[tokio::test]
    #[ignore = "Temporarily disable this test as it depends on external network which may cause \
                timeout"]
    async fn test_extension_manifest_with_interface_remote_path() {
        let manifest_file_path =
            Path::new("tests/test_data/extension_manifest_with_remote_interface.json");

        let manifest = parse_manifest_from_file(manifest_file_path).await.unwrap();

        let api = manifest.api.as_ref().unwrap();
        assert!(api.interface.is_some());
        assert_eq!(api.interface.as_ref().unwrap().len(), 1);

        let interface = api.interface.as_ref().unwrap()[0].clone();
        assert_eq!(interface.import_uri, "https://raw.githubusercontent.com/TEN-framework/ten-framework/0.10.16/core/src/ten_rust/tests/test_data/interface/foo_1.json");

        // Check the flattened manifest api.
        let flattened_manifest = manifest.get_flattened_api(None).await.unwrap();
        let flattened_manifest = flattened_manifest.as_ref().unwrap();
        assert!(flattened_manifest.interface.is_none());

        // Check the properties.
        let property = flattened_manifest.property.as_ref().unwrap();
        let properties = property.properties.as_ref().unwrap();
        assert_eq!(properties.len(), 3);
        assert_eq!(properties.get("foo").unwrap().prop_type, ValueType::Bool);
        assert_eq!(properties.get("bar").unwrap().prop_type, ValueType::Int64);
        assert_eq!(properties.get("key").unwrap().prop_type, ValueType::String);

        // Check the cmd_in/cmd_out/data_in/data_out/audio_frame_in/
        // audio_frame_out/video_frame_in/video_frame_out.
        assert_eq!(flattened_manifest.cmd_in.as_ref().unwrap().len(), 3);
        assert_eq!(flattened_manifest.cmd_out.as_ref().unwrap().len(), 2);
        assert_eq!(flattened_manifest.data_in.as_ref().unwrap().len(), 1);
        assert!(flattened_manifest.data_out.is_none());
        assert!(flattened_manifest.audio_frame_in.is_none());
        assert!(flattened_manifest.audio_frame_out.is_none());
        assert!(flattened_manifest.video_frame_in.is_none());
        assert!(flattened_manifest.video_frame_out.is_none());
    }

    #[tokio::test]
    async fn test_extension_manifest_with_interface_local_path() {
        // Create a temporary file.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        // Write the json file.
        let foo_1_json_str = include_str!("../../../test_data/interface/foo_1.json");

        let foo_2_json_str = include_str!("../../../test_data/interface/foo_2.json");

        let foo_1_file_path = std::path::Path::new(&test_dir).join("foo_1.json");

        let foo_2_file_path = std::path::Path::new(&test_dir).join("foo_2.json");

        // Write the json file to the temporary file.
        std::fs::write(&foo_1_file_path, foo_1_json_str).unwrap();
        std::fs::write(&foo_2_file_path, foo_2_json_str).unwrap();

        let interface_uri = format!("file://{}", foo_1_file_path.display());

        let manifest_str =
            include_str!("../../../test_data/extension_manifest_with_local_file_interface.json");

        let mut manifest: Manifest = Manifest::create_from_str(manifest_str).unwrap();

        let api = manifest.api.as_mut().unwrap();
        api.interface.as_mut().unwrap()[0].import_uri = interface_uri;

        // Check the flattened manifest api.
        let flattened_manifest = manifest.get_flattened_api(None).await.unwrap();
        let flattened_manifest = flattened_manifest.as_ref().unwrap();

        // Check the properties.
        let property = flattened_manifest.property.as_ref().unwrap();
        let properties = property.properties.as_ref().unwrap();
        assert_eq!(properties.len(), 3);
        assert_eq!(properties.get("foo").unwrap().prop_type, ValueType::Bool);
        assert_eq!(properties.get("bar").unwrap().prop_type, ValueType::Int64);
        assert_eq!(properties.get("key").unwrap().prop_type, ValueType::String);

        // Check the cmd_in/cmd_out/data_in/data_out/audio_frame_in/
        // audio_frame_out/video_frame_in/video_frame_out.
        assert_eq!(flattened_manifest.cmd_in.as_ref().unwrap().len(), 3);
        assert_eq!(flattened_manifest.cmd_out.as_ref().unwrap().len(), 2);
        assert_eq!(flattened_manifest.data_in.as_ref().unwrap().len(), 1);
        assert!(flattened_manifest.data_out.is_none());
        assert!(flattened_manifest.audio_frame_in.is_none());
        assert!(flattened_manifest.audio_frame_out.is_none());
        assert!(flattened_manifest.video_frame_in.is_none());
        assert!(flattened_manifest.video_frame_out.is_none());
    }

    #[tokio::test]
    async fn test_extension_manifest_with_no_existing_interface() {
        // Create a temporary file.
        let temp_dir = tempfile::tempdir().unwrap();
        let test_dir = temp_dir.path().to_str().unwrap().to_string();

        let foo_1_file_path = std::path::Path::new(&test_dir).join("foo_1.json");

        let interface_uri = format!("file://{}", foo_1_file_path.display());

        // Create a manifest with a non-existing interface.
        let manifest_str =
            include_str!("../../../test_data/extension_manifest_with_local_file_interface.json");

        let mut manifest: Manifest = Manifest::create_from_str(manifest_str).unwrap();

        let api = manifest.api.as_mut().unwrap();
        api.interface.as_mut().unwrap()[0].import_uri = interface_uri;

        // Check the flattened manifest api.
        let result = manifest.get_flattened_api(None).await;
        assert!(result.is_err());
    }
}
