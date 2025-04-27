//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::path::PathBuf;
    use ten_manager::registry::local::*;

    #[test]
    fn test_check_file_exists() {
        // Test file that doesn't exist.
        let result = check_file_exists(&PathBuf::from("non_existent_file.txt"));
        assert!(result.is_err());
    }

    #[test]
    fn test_extract_filename_from_path() {
        // Test with simple filename
        let result = extract_filename_from_path(&PathBuf::from("filename.txt"));
        assert_eq!(result, Some("filename.txt".to_string()));

        // Test with a path
        let result =
            extract_filename_from_path(&PathBuf::from("path/to/filename.txt"));
        assert_eq!(result, Some("filename.txt".to_string()));

        // Test with just a directory ending with a slash
        // Note: The function will return the last component even if it's a
        // directory
        let path_with_trailing_slash = if cfg!(windows) {
            PathBuf::from("path\\to\\")
        } else {
            PathBuf::from("path/to")
        };
        let result = extract_filename_from_path(&path_with_trailing_slash);
        assert_eq!(result, Some("to".to_string()));
    }
}
