//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod store;

#[cfg(test)]
mod tests {
    use ten_rust::schema::runtime_interface::{
        create_schema_from_json, create_schema_from_json_str,
    };

    #[test]
    fn test_create_schema() {
        let schema_json = serde_json::json!({
            "type": "object",
            "properties": {
                "name": {
                    "type": "string"
                },
                "age": {
                    "type": "int8"
                }
            }
        });

        let schema = create_schema_from_json(&schema_json).unwrap();
        assert!(!schema.as_ptr().is_null());
    }

    #[test]
    fn test_create_schema_invalid_json() {
        let schema_str = include_str!("../../test_data/invalid_schema.json");
        let schema_result = create_schema_from_json_str(schema_str);
        println!("{:?}", schema_result);
        assert!(schema_result.is_err());
    }

    #[test]
    fn test_schema_compatible() {
        let schema1_json = serde_json::json!({
            "type": "object",
            "properties": {
                "name": {
                    "type": "string"
                },
                "age": {
                    "type": "int8"
                }
            },
            "required": ["name"]
        });
        let schema1 = create_schema_from_json(&schema1_json).unwrap();

        let schema2_json = serde_json::json!({
            "type": "object",
            "properties": {
                "name": {
                    "type": "string"
                },
                "age": {
                    "type": "int16"
                }
            }
        });
        let schema2 = create_schema_from_json(&schema2_json).unwrap();

        let result = schema1.is_compatible_with(&schema2);
        assert!(result.is_ok());
    }

    #[test]
    fn test_schema_clone() {
        let schema_json = serde_json::json!({
            "type": "object",
            "properties": {
                "name": {
                    "type": "string"
                },
                "age": {
                    "type": "int8"
                }
            }
        });

        let schema = create_schema_from_json(&schema_json).unwrap();
        assert!(!schema.as_ptr().is_null());

        let cloned_schema = schema.clone();
        assert!(!cloned_schema.as_ptr().is_null());
    }
}
