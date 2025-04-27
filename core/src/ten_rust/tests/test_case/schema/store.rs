//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::str::FromStr;

    use ten_rust::{pkg_info::manifest::Manifest, schema::store::SchemaStore};

    #[test]
    fn test_create_schema_store_success() {
        let manifest_str = include_str!(
            "../../test_data/extension_manifest_has_all_types_schema.json"
        );

        let manifest_result = Manifest::from_str(manifest_str);
        let manifest = manifest_result.as_ref().unwrap();
        let mut schema_store = SchemaStore::default();
        schema_store
            .parse_schemas_from_manifest(manifest.api.as_ref().unwrap())
            .unwrap();

        assert!(schema_store.property.is_some());

        let property = serde_json::json!({"app_id": "123", "app_version": 1});
        let property_schema = schema_store.property.as_ref().unwrap();
        assert!(property_schema.validate_json(&property).is_ok());
    }
}
