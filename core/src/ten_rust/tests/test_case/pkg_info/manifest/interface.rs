//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::path::Path;

    use ten_rust::pkg_info::manifest::parse_manifest_from_file;
    use ten_rust::pkg_info::value_type::ValueType;

    #[tokio::test]
    async fn test_extension_manifest_with_interface_from_str() {
        let manifest_file_path =
            Path::new("tests/test_data/extension_manifest_with_interface.json");

        let manifest =
            parse_manifest_from_file(manifest_file_path).await.unwrap();

        let api = manifest.api.as_ref().unwrap();
        assert!(api.interface.is_some());
        assert_eq!(api.interface.as_ref().unwrap().len(), 1);

        let interface = api.interface.as_ref().unwrap()[0].clone();
        assert_eq!(interface.import_uri, "./interface/foo_1.json");

        // Check the flattened manifest api.
        let flattened_manifest = manifest.get_flattened_api().await.unwrap();
        let flattened_manifest = flattened_manifest.as_ref().unwrap();
        assert!(flattened_manifest.interface.is_none());

        // Check the properties.
        let property = flattened_manifest.property.as_ref().unwrap();
        assert_eq!(property.len(), 3);
        assert_eq!(property.get("foo").unwrap().prop_type, ValueType::Bool);
        assert_eq!(property.get("bar").unwrap().prop_type, ValueType::Int64);
        assert_eq!(property.get("key").unwrap().prop_type, ValueType::String);

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

    // TODO(xilin): Add more tests for the interface. http/https url, file path,
    // etc.
}
