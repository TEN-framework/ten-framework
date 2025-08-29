//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use semver::Version;
    use serde_json::{json, Value};
    use ten_rust::pkg_info::{
        hash::gen_hash_hex,
        manifest::{support::ManifestSupport, Manifest},
        pkg_type::PkgType,
        supports::{Arch, Os},
        PkgInfo,
    };

    #[test]
    fn test_gen_hash_hex_without_supports() {
        let pkg_type = PkgType::Extension;
        let name = "test_extension";
        let version = Version::parse("1.0.0").unwrap();
        let supports = &[];

        let hash = gen_hash_hex(&pkg_type, name, &version, supports);

        // Print the JSON content for verification
        let json_obj = json!({
            "type": pkg_type.to_string(),
            "name": name.to_owned(),
            "version": version.to_string(),
        });
        let json_string = serde_json::to_string_pretty(&json_obj).unwrap();
        println!("JSON without supports:");
        println!("{json_string}");
        println!("Generated hash: {hash}");

        assert!(!hash.is_empty());
        assert_eq!(hash.len(), 64); // SHA256 hash is 64 hex characters
    }

    #[test]
    fn test_gen_hash_hex_with_supports() {
        let pkg_type = PkgType::Extension;
        let name = "test_extension";
        let version = Version::parse("1.0.0").unwrap();
        let supports = vec![
            ManifestSupport {
                os: Some(Os::Linux),
                arch: Some(Arch::X64),
            },
            ManifestSupport {
                os: Some(Os::Mac),
                arch: Some(Arch::X64),
            },
        ];

        let hash = gen_hash_hex(&pkg_type, name, &version, &supports);

        // Print the JSON content for verification
        let mut json_obj = json!({
            "type": pkg_type.to_string(),
            "name": name.to_owned(),
            "version": version.to_string(),
        });

        let supports_array: Vec<Value> = supports
            .iter()
            .map(|support| {
                let mut support_obj = serde_json::Map::new();
                if let Some(os) = &support.os {
                    support_obj.insert("os".to_string(), Value::String(os.to_string()));
                }
                if let Some(arch) = &support.arch {
                    support_obj.insert("arch".to_string(), Value::String(arch.to_string()));
                }
                Value::Object(support_obj)
            })
            .collect();
        json_obj["supports"] = Value::Array(supports_array);

        let json_string = serde_json::to_string_pretty(&json_obj).unwrap();
        println!("JSON with supports:");
        println!("{json_string}");
        println!("Generated hash: {hash}");

        assert!(!hash.is_empty());
        assert_eq!(hash.len(), 64); // SHA256 hash is 64 hex characters
    }

    #[test]
    fn test_pkg_info_gen_hash_hex() {
        // Create a simple manifest for testing
        let manifest_json = r#"
        {
            "type": "extension",
            "name": "test_extension",
            "version": "1.2.3",
            "supports": [
                {
                    "os": "linux",
                    "arch": "x64"
                },
                {
                    "os": "mac",
                    "arch": "x64"
                }
            ]
        }
        "#;

        let manifest: Manifest = serde_json::from_str(manifest_json).unwrap();
        let pkg_info = PkgInfo {
            manifest: manifest.clone(),
            property: None,
            compatible_score: 0,
            is_installed: false,
            url: "/test/path".to_string(),
            hash: String::new(), // Will be calculated
            schema_store: None,
            is_local_dependency: false,
            local_dependency_path: None,
            local_dependency_base_dir: None,
        };

        let hash = pkg_info.gen_hash_hex();

        // Print the expected JSON content for verification
        let supports = pkg_info.manifest.supports.as_ref().unwrap();
        let mut json_obj = json!({
            "type": pkg_info.manifest.type_and_name.pkg_type.to_string(),
            "name": pkg_info.manifest.type_and_name.name.clone(),
            "version": pkg_info.manifest.version.to_string(),
        });

        let supports_array: Vec<Value> = supports
            .iter()
            .map(|support| {
                let mut support_obj = serde_json::Map::new();
                if let Some(os) = &support.os {
                    support_obj.insert("os".to_string(), Value::String(os.to_string()));
                }
                if let Some(arch) = &support.arch {
                    support_obj.insert("arch".to_string(), Value::String(arch.to_string()));
                }
                Value::Object(support_obj)
            })
            .collect();
        json_obj["supports"] = Value::Array(supports_array);

        let json_string = serde_json::to_string_pretty(&json_obj).unwrap();
        println!("PkgInfo JSON:");
        println!("{json_string}");
        println!("PkgInfo generated hash: {hash}");

        assert!(!hash.is_empty());
        assert_eq!(hash.len(), 64); // SHA256 hash is 64 hex characters
    }

    #[test]
    fn test_gen_hash_hex_with_partial_supports() {
        let pkg_type = PkgType::Extension;
        let name = "test_extension";
        let version = Version::parse("1.0.0").unwrap();
        let supports = vec![
            ManifestSupport {
                os: Some(Os::Linux),
                arch: None, // Test with missing arch
            },
            ManifestSupport {
                os: None, // Test with missing os
                arch: Some(Arch::X64),
            },
        ];

        let hash = gen_hash_hex(&pkg_type, name, &version, &supports);

        // Print the JSON content for verification
        let mut json_obj = json!({
            "type": pkg_type.to_string(),
            "name": name.to_owned(),
            "version": version.to_string(),
        });

        let supports_array: Vec<Value> = supports
            .iter()
            .map(|support| {
                let mut support_obj = serde_json::Map::new();
                if let Some(os) = &support.os {
                    support_obj.insert("os".to_string(), Value::String(os.to_string()));
                }
                if let Some(arch) = &support.arch {
                    support_obj.insert("arch".to_string(), Value::String(arch.to_string()));
                }
                Value::Object(support_obj)
            })
            .collect();
        json_obj["supports"] = Value::Array(supports_array);

        let json_string = serde_json::to_string_pretty(&json_obj).unwrap();
        println!("JSON with partial supports:");
        println!("{json_string}");
        println!("Generated hash: {hash}");

        assert!(!hash.is_empty());
        assert_eq!(hash.len(), 64); // SHA256 hash is 64 hex characters
    }
}
