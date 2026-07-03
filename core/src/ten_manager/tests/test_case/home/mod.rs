//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use ten_manager::home::get_home_dir;

    use crate::test_case::common::temp_home::{with_temp_home_dir, TempHome};

    #[test]
    fn test_temp_home_sets_tman_test_home() {
        let temp_home = TempHome::new();

        // Verify TEN_MANAGER_HOME_INTERNAL_USE_ONLY is set to the temp
        // directory
        let test_home = std::env::var("TEN_MANAGER_HOME_INTERNAL_USE_ONLY")
            .expect("TEN_MANAGER_HOME_INTERNAL_USE_ONLY should be set");
        assert_eq!(test_home, temp_home.path().to_string_lossy());

        // Verify get_home_dir uses the test home
        let home_dir = get_home_dir();
        let expected_path = if cfg!(target_os = "windows") {
            temp_home.path().join("AppData").join("Roaming").join("tman")
        } else {
            temp_home.path().join(".tman")
        };
        assert_eq!(home_dir, expected_path);
    }

    #[test]
    fn test_with_temp_home_dir_function() {
        with_temp_home_dir(|| {
            // Inside the closure, TEN_MANAGER_HOME_INTERNAL_USE_ONLY should be
            // set
            let test_home = std::env::var("TEN_MANAGER_HOME_INTERNAL_USE_ONLY")
                .expect("TEN_MANAGER_HOME_INTERNAL_USE_ONLY should be set");
            assert!(!test_home.is_empty());

            // get_home_dir should use the test home
            let home_dir = get_home_dir();
            let test_path = PathBuf::from(&test_home);
            let expected_path = if cfg!(target_os = "windows") {
                test_path.join("AppData").join("Roaming").join("tman")
            } else {
                test_path.join(".tman")
            };
            assert_eq!(home_dir, expected_path);
        });
    }
}
