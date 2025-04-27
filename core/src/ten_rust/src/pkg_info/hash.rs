//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use semver::Version;
use sha2::{Digest, Sha256};

use super::{
    manifest::dependency::ManifestDependency,
    manifest::support::ManifestSupport, pkg_type::PkgType, PkgInfo,
};

impl PkgInfo {
    pub fn gen_hash_hex(&self) -> String {
        // Get dependencies or use empty vector if None
        let dependencies = self.manifest.dependencies.as_ref().map_or_else(
            || &[] as &[ManifestDependency],
            |deps| deps.as_slice(),
        );

        // Get supports or use empty vector if None
        let supports = self.manifest.supports.as_ref().map_or_else(
            || &[] as &[ManifestSupport],
            |supports| supports.as_slice(),
        );

        gen_hash_hex(
            &self.manifest.type_and_name.pkg_type,
            &self.manifest.type_and_name.name,
            &self.manifest.version,
            dependencies,
            supports,
        )
    }
}

fn gen_hash_hex(
    pkg_type: &PkgType,
    name: &String,
    version: &Version,
    dependencies: &[ManifestDependency],
    supports: &[ManifestSupport],
) -> String {
    let mut hasher = Sha256::new();

    // Hash type.
    let type_string = format!("{}", pkg_type);
    hasher.update(type_string);

    // Hash name.
    hasher.update(name);

    // Hash version.
    let version_string = version.to_string();
    hasher.update(version_string);

    // Hash dependencies.
    for dep in dependencies {
        let dep_string = match dep {
            ManifestDependency::RegistryDependency {
                pkg_type,
                name,
                version_req,
            } => {
                format!("{}:{}@{}", pkg_type, name, version_req)
            }
            ManifestDependency::LocalDependency { path, base_dir } => {
                format!("local:{}@{}", path, base_dir)
            }
        };
        hasher.update(dep_string);
    }

    // Hash supports.
    for support in supports {
        let support_string = format!("{}", support);
        hasher.update(support_string);
    }

    let hash_result = hasher.finalize();
    let hash_hex = format!("{:x}", hash_result);

    hash_hex
}
