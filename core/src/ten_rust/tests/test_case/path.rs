//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::utils::path::get_real_path_from_import_uri;

    #[test]
    fn test_get_real_path_absolute_path() {
        // Test with Unix-style absolute path - this should still fail because
        // absolute paths are only allowed after variable replacement
        #[cfg(unix)]
        {
            let import_uri = "/home/user/interface.json";
            let base_dir = "/home/user";
            let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

            assert!(real_path.is_err());
            assert!(real_path
                .err()
                .unwrap()
                .to_string()
                .contains("Absolute paths are not supported in import_uri"));
        }
    }

    #[test]
    fn test_get_real_path_http_url() {
        let import_uri = "http://example.com/api/interface.json";
        let base_dir = "/some/path";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "http://example.com/api/interface.json");
    }

    #[test]
    fn test_get_real_path_https_url() {
        let import_uri = "https://example.com/api/interface.json";
        let base_dir = "/some/path";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "https://example.com/api/interface.json");
    }

    #[test]
    fn test_get_real_path_file_url() {
        let import_uri = "file:///home/user/interface.json";
        let base_dir = "/some/path";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "file:///home/user/interface.json");
    }

    #[test]
    fn test_get_real_path_unsupported_url_scheme() {
        let import_uri = "ftp://example.com/interface.json";
        let base_dir = "/some/path";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_err());
        assert!(real_path.err().unwrap().to_string().contains("Unsupported URL scheme 'ftp'"));
    }

    #[test]
    fn test_get_real_path_relative_path_empty_base_dir() {
        let import_uri = "interface.json";
        let base_dir = "";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_err());
        assert!(real_path
            .err()
            .unwrap()
            .to_string()
            .contains("base_dir cannot be None when uri is a relative path"));
    }

    #[test]
    fn test_get_real_path_relative_path_with_http_base_dir() {
        let import_uri = "interface.json";
        let base_dir = "http://example.com/api/v1";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "http://example.com/api/v1/interface.json");
    }

    #[test]
    fn test_get_real_path_relative_path_with_https_base_dir() {
        let import_uri = "interface.json";
        let base_dir = "https://example.com/api/v1";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "https://example.com/api/v1/interface.json");
    }

    #[test]
    fn test_get_real_path_relative_path_with_https_base_dir2() {
        let import_uri = "./interface.json";
        let base_dir = "https://example.com/api/v1";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "https://example.com/api/v1/interface.json");
    }

    #[test]
    fn test_get_real_path_relative_path_with_https_base_dir_and_relative_path() {
        let import_uri = "../interface.json";
        let base_dir = "https://example.com/api/v1";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "https://example.com/api/interface.json");
    }

    #[test]
    fn test_get_real_path_relative_path_with_file_base_dir() {
        let import_uri = "../interface.json";
        let base_dir = "file:///home/user/tmp";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "file:///home/user/interface.json");
    }

    #[test]
    fn test_get_real_path_relative_path_with_local_base_dir() {
        let import_uri = "interface.json";
        let base_dir = "/home/user";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        let result = real_path.unwrap();

        // On Windows, the path might be normalized differently
        // Just check that it ends with the expected filename and contains the
        // base components
        assert!(result.contains("interface.json"));
        assert!(result.contains("home") || result.contains("user"));

        // For Unix-like systems, check the exact path
        #[cfg(unix)]
        assert_eq!(result, "/home/user/interface.json");
    }

    #[test]
    fn test_get_real_path_relative_path_with_subdirectory() {
        let import_uri = "subdir/interface.json";
        let base_dir = "/home/user";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        let result = real_path.unwrap();

        // Check that the path contains the expected components
        assert!(result.contains("interface.json"));
        assert!(result.contains("subdir"));
        assert!(result.contains("home") || result.contains("user"));

        // For Unix-like systems, check the exact path
        #[cfg(unix)]
        assert_eq!(result, "/home/user/subdir/interface.json");
    }

    #[test]
    fn test_get_real_path_relative_path_with_parent_directory() {
        let import_uri = "../interface.json";
        let base_dir = "/home/user/project";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        let result = real_path.unwrap();

        // Check that the path contains the expected components
        assert!(result.contains("interface.json"));
        assert!(result.contains("home") || result.contains("user"));
        // Should not contain "project" since we went up one level

        // For Unix-like systems, check the exact path
        #[cfg(unix)]
        assert_eq!(result, "/home/user/interface.json");
    }

    #[test]
    fn test_get_real_path_windows_absolute_path() {
        // Test with Windows-style absolute path on all platforms
        #[cfg(windows)]
        {
            let import_uri_win = "C:\\Users\\test\\interface.json";
            let base_dir_win = "C:\\Users\\test";
            let real_path_win =
                get_real_path_from_import_uri(import_uri_win, Some(base_dir_win), None);

            assert!(real_path_win.is_err());
            assert!(real_path_win
                .err()
                .unwrap()
                .to_string()
                .contains("Absolute paths are not supported in import_uri"));
        }
    }

    #[test]
    fn test_get_real_path_complex_relative_path() {
        let import_uri = "./subdir/../interface.json";
        let base_dir = "/home/user/project";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        let result = real_path.unwrap();

        // Check that the path contains the expected components
        assert!(result.contains("interface.json"));
        assert!(result.contains("project"));
        assert!(result.contains("home") || result.contains("user"));

        // For Unix-like systems, check the exact path
        #[cfg(unix)]
        assert_eq!(result, "/home/user/project/interface.json");
    }

    #[test]
    fn test_get_real_path_url_with_query_params() {
        let import_uri = "https://example.com/api/interface.json?version=1.0";
        let base_dir = "/some/path";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "https://example.com/api/interface.json?version=1.0");
    }

    #[test]
    fn test_get_real_path_url_with_fragment() {
        let import_uri = "https://example.com/api/interface.json#section1";
        let base_dir = "/some/path";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        assert_eq!(real_path.unwrap(), "https://example.com/api/interface.json#section1");
    }

    #[test]
    fn test_get_real_path_relative_with_windows_base_dir() {
        let import_uri = "interface.json";
        let base_dir = "C:\\Users\\test";
        let real_path = get_real_path_from_import_uri(import_uri, Some(base_dir), None);

        assert!(real_path.is_ok());
        // On Windows, this would be C:\Users\test\interface.json
        // On Unix-like systems, it would be C:\Users\test/interface.json
        assert!(real_path.unwrap().contains("interface.json"));
    }

    #[test]
    fn test_get_real_path_with_app_base_dir_variable() {
        let import_uri = "${app_base_dir}/ten_packages/interface.json";
        let base_dir = None;

        #[cfg(unix)]
        {
            let app_base_dir = "/home/user/myapp";
            let real_path = get_real_path_from_import_uri(import_uri, base_dir, Some(app_base_dir));

            assert!(real_path.is_ok());
            assert_eq!(real_path.unwrap(), "file:///home/user/myapp/ten_packages/interface.json");
        }

        #[cfg(windows)]
        {
            let app_base_dir = "C:\\Users\\test\\myapp";
            let real_path = get_real_path_from_import_uri(import_uri, base_dir, Some(app_base_dir));

            assert!(real_path.is_ok());
            let result = real_path.unwrap();
            assert!(result.starts_with("file://"));
            assert!(result.contains("ten_packages"));
            assert!(result.contains("interface.json"));
            // On Windows, check that the path contains the expected components
            assert!(
                result.contains("Users") || result.contains("test") || result.contains("myapp")
            );
        }
    }

    #[test]
    fn test_get_real_path_with_app_base_dir_variable_no_app_base_dir() {
        let import_uri = "${app_base_dir}/ten_packages/interface.json";
        let base_dir = None;
        let app_base_dir = None;
        let real_path = get_real_path_from_import_uri(import_uri, base_dir, app_base_dir);

        assert!(real_path.is_err());
        assert!(real_path.err().unwrap().to_string().contains(
            "app_base_dir must be provided when import_uri contains ${app_base_dir} variable"
        ));
    }

    #[test]
    fn test_get_real_path_with_app_base_dir_variable_relative_path() {
        let import_uri = "${app_base_dir}/ten_packages/../config/interface.json";
        let base_dir = None;

        #[cfg(unix)]
        {
            let app_base_dir = "/home/user/myapp";
            let real_path = get_real_path_from_import_uri(import_uri, base_dir, Some(app_base_dir));

            assert!(real_path.is_ok());
            assert_eq!(real_path.unwrap(), "file:///home/user/myapp/config/interface.json");
        }

        #[cfg(windows)]
        {
            let app_base_dir = "C:\\Users\\test\\myapp";
            let real_path = get_real_path_from_import_uri(import_uri, base_dir, Some(app_base_dir));

            assert!(real_path.is_ok());
            let result = real_path.unwrap();
            assert!(result.starts_with("file://"));
            assert!(result.contains("config"));
            assert!(result.contains("interface.json"));
            // On Windows, check that the path contains the expected components
            assert!(
                result.contains("Users") || result.contains("test") || result.contains("myapp")
            );
            // Should not contain "ten_packages" since we went up one level with ../
            assert!(!result.contains("ten_packages"));
        }
    }

    #[test]
    fn test_get_real_path_with_app_base_dir_variable_windows_path() {
        let import_uri = "${app_base_dir}/ten_packages/interface.json";
        let base_dir = None;
        let app_base_dir = "C:\\Users\\test\\myapp";
        let real_path = get_real_path_from_import_uri(import_uri, base_dir, Some(app_base_dir));

        if real_path.is_err() {
            panic!("Expected Ok but got error: {}", real_path.err().unwrap());
        }
        let result = real_path.unwrap();
        assert!(result.starts_with("file://"));
        assert!(result.contains("ten_packages"));
        assert!(result.contains("interface.json"));
        // On Windows, the path separator might be normalized to /
        assert!(result.contains("Users") || result.contains("test") || result.contains("myapp"));
    }

    #[test]
    fn test_get_real_path_without_app_base_dir_variable() {
        let import_uri = "ten_packages/interface.json";
        let base_dir = "/home/user/myapp";
        let app_base_dir = "/some/other/path";
        let real_path =
            get_real_path_from_import_uri(import_uri, Some(base_dir), Some(app_base_dir));

        assert!(real_path.is_ok());
        let result = real_path.unwrap();

        // Should use base_dir, not app_base_dir, when no variable is present
        #[cfg(unix)]
        assert_eq!(result, "/home/user/myapp/ten_packages/interface.json");
    }
}
