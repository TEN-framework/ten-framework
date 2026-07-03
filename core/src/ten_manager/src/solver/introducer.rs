//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::{HashMap, HashSet};

use anyhow::Result;
use regex::Regex;
use ten_rust::pkg_info::{
    manifest::dependency::ManifestDependency, pkg_basic_info::PkgBasicInfo,
    pkg_type_and_name::PkgTypeAndName, PkgInfo,
};

use crate::dep_and_candidate::get_pkg_info_from_candidates;

/// Returns a map from a package to its introducer and the requested version.
pub fn extract_introducer_relations_from_raw_solver_results(
    results: &[String],
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
) -> Result<HashMap<PkgBasicInfo, (String, Option<PkgInfo>)>> {
    let re =
        Regex::new(r#"introducer\("([^"]+)","([^"]+)","([^"]+)","([^"]+)","([^"]*)","([^"]*)"\)"#)
            .unwrap();

    let mut introducer_relations: HashMap<PkgBasicInfo, (String, Option<PkgInfo>)> = HashMap::new();

    for result in results.iter() {
        if let Some(caps) = re.captures(result) {
            let pkg_type_str = caps.get(1).unwrap().as_str();
            let name = caps.get(2).unwrap().as_str();
            let version_str = caps.get(3).unwrap().as_str();

            let introducer_type_str = caps.get(4).unwrap().as_str();
            let introducer_name = caps.get(5).unwrap().as_str();
            let introducer_version_str = caps.get(6).unwrap().as_str();

            // Using the requested version (i.e., the version field declared in
            // manifest.json) in the dependency chain is more user-friendly.
            let mut requested_version_str = version_str.to_string();

            let pkg_info =
                get_pkg_info_from_candidates(pkg_type_str, name, version_str, all_candidates)?;

            let introducer = if introducer_type_str == "root" {
                None
            } else {
                let introducer_info = get_pkg_info_from_candidates(
                    introducer_type_str,
                    introducer_name,
                    introducer_version_str,
                    all_candidates,
                )?;

                let requested_dep_in_introducer =
                    introducer_info.get_dependency_by_type_and_name(pkg_type_str, name);
                if let Some(requested_dep_in_introducer) = requested_dep_in_introducer {
                    // The `version` declared in the `dependencies` section in
                    // manifest.json is always present.
                    requested_version_str = match requested_dep_in_introducer {
                        ManifestDependency::RegistryDependency {
                            version_req, ..
                        } => version_req.as_processed().to_string(),
                        ManifestDependency::LocalDependency {
                            ..
                        } => version_str.to_string(),
                    };
                } else {
                    return Err(anyhow::anyhow!(
                        "Requested dependency [{}]{} not found in introducer, should not happen.",
                        pkg_type_str,
                        name
                    ));
                }

                Some(introducer_info.clone())
            };

            introducer_relations.insert((&pkg_info).into(), (requested_version_str, introducer));
        }
    }

    Ok(introducer_relations)
}

pub fn get_dependency_chain(
    introducer: &PkgInfo,
    conflict_pkg_identity: &PkgTypeAndName,
    introducer_relations: &HashMap<PkgBasicInfo, (String, Option<PkgInfo>)>,
) -> Vec<(String, PkgTypeAndName)> {
    let mut chain = Vec::new();
    let mut current_pkg = introducer.clone();
    let mut visited = HashSet::<PkgBasicInfo>::new();

    // Add the leaf node (i.e., `introducer` depends on `pkg_name`) to the
    // dependency chain first.
    let requested_dep_in_introducer = introducer
        .get_dependency_by_type_and_name(
            &conflict_pkg_identity.pkg_type.to_string(),
            &conflict_pkg_identity.name,
        )
        .unwrap();

    let version_req_str = match requested_dep_in_introducer {
        ManifestDependency::RegistryDependency {
            version_req, ..
        } => version_req.as_processed().to_string(),
        ManifestDependency::LocalDependency {
            ..
        } => {
            // Use wildcard for local deps.
            "*".to_string()
        }
    };

    chain.push((version_req_str, conflict_pkg_identity.clone()));

    loop {
        if !visited.insert((&current_pkg).into()) {
            // Detected a cycle, break to prevent infinite loop.
            break;
        }

        match introducer_relations.get(&(&current_pkg).into()) {
            Some((requested_version, Some(introducer_pkg))) => {
                // The dependency chain is that `introducer_pkg` depends on
                // `current_pkg`@`requested_version`, i.e., the
                // `requested_version` is used to declared the `current_pkg` in
                // the manifest.json of `introducer_pkg`.
                chain.push((requested_version.clone(), (&current_pkg).into()));
                current_pkg = introducer_pkg.clone();
            }
            Some((requested_version, None)) => {
                // Reached the root.
                chain.push((requested_version.clone(), (&current_pkg).into()));
                break;
            }
            None => {
                panic!("Introducer not found, possibly an error.");
            }
        }
    }

    // Reverse the chain to start from the root.
    chain.reverse();
    chain
}
