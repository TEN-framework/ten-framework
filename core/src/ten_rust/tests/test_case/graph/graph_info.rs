//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::fs;
    use tempfile::tempdir;
    use ten_rust::graph::graph_info::load_graph_from_uri;

    #[test]
    fn test_load_graph_from_file_url() {
        // Create a temporary directory and file
        let temp_dir = tempdir().unwrap();
        let file_path = temp_dir.path().join("test_graph.json");

        // Create a simple test graph
        let test_graph = r#"{
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension",
                    "addon": "test_addon",
                    "app": "localhost"
                }
            ]
        }"#;

        fs::write(&file_path, test_graph).unwrap();

        // Create a file:// URL
        let file_url = format!("file://{}", file_path.display());

        // Test loading the graph
        let mut new_base_dir = Some(String::new());
        let result = load_graph_from_uri(&file_url, None, &mut new_base_dir);

        assert!(result.is_ok());
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 1);
        assert_eq!(graph.nodes[0].name, "test_extension");

        // Check that new_base_dir was set correctly
        assert!(new_base_dir.is_some());
        let base_dir = new_base_dir.unwrap();
        assert_eq!(base_dir, temp_dir.path().to_string_lossy());
    }

    #[test]
    fn test_load_graph_from_relative_path() {
        // Create a temporary directory and file
        let temp_dir = tempdir().unwrap();
        let file_path = temp_dir.path().join("test_graph.json");

        // Create a simple test graph
        let test_graph = r#"{
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension",
                    "addon": "test_addon",
                    "app": "localhost"
                }
            ]
        }"#;

        fs::write(&file_path, test_graph).unwrap();

        // Test loading with relative path
        let mut new_base_dir = Some(String::new());
        let result = load_graph_from_uri(
            "test_graph.json",
            Some(&temp_dir.path().to_string_lossy()),
            &mut new_base_dir,
        );

        assert!(result.is_ok());
        let graph = result.unwrap();
        assert_eq!(graph.nodes.len(), 1);
        assert_eq!(graph.nodes[0].name, "test_extension");

        // Check that new_base_dir was set correctly
        assert!(new_base_dir.is_some());
        let base_dir = new_base_dir.unwrap();
        assert_eq!(base_dir, temp_dir.path().to_string_lossy());
    }

    #[test]
    fn test_absolute_path_not_supported() {
        // Test that absolute paths are rejected
        let mut new_base_dir = Some(String::new());

        // Use platform-appropriate absolute path
        #[cfg(unix)]
        let absolute_path = "/absolute/path/to/graph.json";
        #[cfg(windows)]
        let absolute_path = "C:\\absolute\\path\\to\\graph.json";

        let result =
            load_graph_from_uri(absolute_path, None, &mut new_base_dir);

        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg.contains("Absolute paths are not supported"));
        assert!(error_msg.contains("Use file:// URI or relative path instead"));
    }

    #[test]
    fn test_unsupported_url_scheme() {
        let mut new_base_dir = Some(String::new());
        let result = load_graph_from_uri(
            "ftp://example.com/graph.json",
            None,
            &mut new_base_dir,
        );

        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg.contains("Unsupported URL scheme 'ftp'"));
    }

    #[test]
    fn test_relative_path_without_base_dir() {
        let mut new_base_dir = Some(String::new());
        let result =
            load_graph_from_uri("test_graph.json", None, &mut new_base_dir);

        assert!(result.is_err());
        let error_msg = result.unwrap_err().to_string();
        assert!(error_msg
            .contains("base_dir cannot be None when uri is a relative path"));
    }
}
