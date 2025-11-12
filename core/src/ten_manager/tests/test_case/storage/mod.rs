//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use serde_json::json;
    use ten_manager::{
        designer::storage::persistent::{read_persistent_storage, write_persistent_storage},
        home::data::get_home_data_path,
    };

    use crate::test_case::common::temp_home::with_temp_home_dir;

    #[test]
    fn test_read_nonexistent_storage() {
        with_temp_home_dir(|| {
            let result = read_persistent_storage();
            assert!(result.is_ok());

            let data = result.unwrap();
            assert!(data.is_object());
            assert_eq!(data.as_object().unwrap().len(), 0);
        });
    }

    #[test]
    fn test_write_and_read_storage() {
        with_temp_home_dir(|| {
            let test_data = json!({"test_key": "test_value", "nested": {"inner": 42}});

            // Write data
            let write_result = write_persistent_storage(&test_data);
            assert!(write_result.is_ok());

            // Read data
            let read_result = read_persistent_storage();
            assert!(read_result.is_ok());

            let read_data = read_result.unwrap();
            assert_eq!(read_data, test_data);
        });
    }

    #[test]
    fn test_storage_path() {
        with_temp_home_dir(|| {
            let path = get_home_data_path();
            let expected_suffix = if cfg!(target_os = "windows") {
                r"AppData\Roaming\tman\data.json"
            } else {
                ".tman/data.json"
            };
            assert!(path.to_string_lossy().ends_with(expected_suffix));
        });
    }
}
