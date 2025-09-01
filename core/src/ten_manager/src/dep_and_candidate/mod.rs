//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    collections::{HashMap, HashSet},
    path::Path,
    sync::Arc,
};

use anyhow::{anyhow, Context, Result};
use semver::{Version, VersionReq};
use ten_rust::pkg_info::{
    constants::MANIFEST_JSON_FILENAME,
    get_pkg_info_from_path,
    manifest::{
        dependency::{ManifestDependency, TenVersionReq},
        parse_manifest_from_file,
        support::{is_manifest_supports_compatible_with, ManifestSupport, SupportsDisplay},
    },
    pkg_basic_info::PkgBasicInfo,
    pkg_type::PkgType,
    pkg_type_and_name::PkgTypeAndName,
    PkgInfo,
};

use super::{home::config::TmanConfig, registry::get_package_list};
use crate::{
    home::config::is_verbose,
    output::TmanOutput,
    registry::{
        found_result::BASIC_SCOPE,
        pkg_list_cache::{is_superset_of, PackageListCache},
    },
};

// TODO(Wei): Should use the union of the semantic versioning rather than the
// union of all version requirements.
#[derive(Default)]
struct MergedVersionReq {
    version_reqs: HashSet<VersionReq>,
}

impl MergedVersionReq {
    fn new(version_req: &VersionReq) -> Self {
        MergedVersionReq {
            version_reqs: HashSet::from([version_req.clone()]),
        }
    }

    /// If there is already the same version requirement, it means there is no
    /// new information, so return false. Otherwise, return true to let the
    /// caller know that there has been a change.
    fn merge(&mut self, version_req: &VersionReq) -> Result<bool> {
        if self.version_reqs.contains(version_req) {
            return Ok(false);
        }

        self.version_reqs.insert(version_req.clone());

        Ok(true)
    }
}

/// If the contents of merged_dependencies are the same before and after the
/// merge, it means merged_dependencies has not changed, so return false.
/// Otherwise, it means there has been a change, so return true. This allows the
/// caller to know that there may be new content and that some actions may need
/// to be taken.
async fn merge_dependency_to_dependencies(
    merged_dependencies: &mut HashMap<PkgTypeAndName, MergedVersionReq>,
    dependency: &ManifestDependency,
) -> Result<bool> {
    // Extract PkgTypeAndName and VersionReq based on enum variant.
    let (pkg_type_name, version_req) = match dependency {
        ManifestDependency::RegistryDependency {
            pkg_type,
            name,
            version_req,
        } => (
            PkgTypeAndName {
                pkg_type: *pkg_type,
                name: name.clone(),
            },
            version_req,
        ),
        ManifestDependency::LocalDependency {
            path,
            base_dir,
            ..
        } => {
            // Get type and name from manifest for local dependency.
            let base_dir_str = base_dir.as_deref().ok_or_else(|| {
                anyhow!("base_dir cannot be None when processing local dependency in merge")
            })?;
            let abs_path = Path::new(base_dir_str).join(path);
            let dep_manifest_path = abs_path.join(MANIFEST_JSON_FILENAME);

            let local_manifest = parse_manifest_from_file(&dep_manifest_path).await?;
            (
                local_manifest.type_and_name.clone(),
                &TenVersionReq::new(local_manifest.version.to_string())?,
            )
        }
    };

    let mut changed = true;

    if let Some(existed_dependency) = merged_dependencies.get_mut(&pkg_type_name) {
        changed = existed_dependency.merge(version_req.as_processed())?;
    } else {
        // This is the first time seeing this dependency.
        merged_dependencies
            .insert(pkg_type_name, MergedVersionReq::new(version_req.as_processed()));
    }

    Ok(changed)
}

async fn process_local_dependency_to_get_candidate(
    dependency: &ManifestDependency,
    all_candidates: &mut HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    new_pkgs_to_be_searched: &mut Vec<PkgInfo>,
) -> Result<()> {
    // We should only call this with a LocalDependency.
    if let ManifestDependency::LocalDependency {
        path,
        base_dir,
        ..
    } = dependency
    {
        // Construct a `PkgInfo` to represent the package corresponding to
        // the specified path.
        let base_dir_str = base_dir
            .as_deref()
            .ok_or_else(|| anyhow!("base_dir cannot be None when processing local dependency"))?;
        let path_str = path.as_str();

        let abs_path = Path::new(base_dir_str)
            .join(path_str)
            .canonicalize()
            .with_context(|| format!("Failed to canonicalize path: {base_dir_str} + {path_str}"))?;

        let mut pkg_info = get_pkg_info_from_path(&abs_path, false, false, &mut None, None).await?;

        pkg_info.is_local_dependency = true;
        pkg_info.local_dependency_path = Some(path.clone());
        pkg_info.local_dependency_base_dir = base_dir.clone();

        let candidate_map = all_candidates.entry((&pkg_info).into()).or_default();

        candidate_map.insert((&pkg_info).into(), pkg_info.clone());

        new_pkgs_to_be_searched.push(pkg_info);

        Ok(())
    } else {
        Err(anyhow!("Expected LocalDependency but got RegistryDependency"))
    }
}

#[allow(clippy::too_many_arguments)]
async fn process_non_local_dependency_to_get_candidate(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    support: &ManifestSupport,
    dependency: &ManifestDependency,
    all_compatible_installed_pkgs: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    all_candidates: &mut HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    new_pkgs_to_be_searched: &mut Vec<PkgInfo>,
    pkg_list_cache: &mut PackageListCache,
    out: &Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    // Extract PkgTypeAndName and VersionReq based on enum variant.
    let (pkg_type_name, version_req) = match dependency {
        ManifestDependency::RegistryDependency {
            pkg_type,
            name,
            version_req,
        } => (
            PkgTypeAndName {
                pkg_type: *pkg_type,
                name: name.clone(),
            },
            version_req.clone(),
        ),
        _ => {
            return Err(anyhow!("Expected RegistryDependency but got LocalDependency"));
        }
    };

    // First, check whether the package list cache has already processed the
    // same or a more permissive version requirement combination.
    if !pkg_list_cache.check_and_update(&pkg_type_name, version_req.as_processed()) {
        // If a covering combination already exists in the package list cache,
        // skip calling `get_package_list`.
        return Ok(());
    }

    // Retrieve all packages from the registry that meet the specified
    // criteria.
    let results = get_package_list(
        tman_config.clone(),
        Some(pkg_type_name.pkg_type),
        Some(pkg_type_name.name.clone()),
        // With the current design, if there is new information, it will
        // definitely be only this version requirement. Although there may be
        // some overlap with previous version requirements, given the current
        // design, this is the best we can do for now. The answer won't be
        // wrong, but the efficiency might be somewhat lower.
        Some(version_req.as_processed().clone()),
        None, // No tag filtering.
        Some(BASIC_SCOPE.iter().map(|s| s.to_string()).collect()),
        None,
        None, // Retrieve all packages.
        out,
    )
    .await?;

    let mut candidate_pkg_infos: Vec<PkgInfo> = vec![];

    // Put all the results returned by the registry into
    // `candidate_pkg_infos`, making it convenient for `supports`
    // filtering later.
    for result in results {
        let mut candidate_pkg_info: PkgInfo = (&result).into();

        candidate_pkg_info.url = result.download_url;
        candidate_pkg_info.is_installed = false;

        candidate_pkg_infos.push(candidate_pkg_info);
    }

    // Find packages from the installed packages that meet the specified
    // criteria and add them to the candidate_pkg_infos. This action ensures
    // that the dependencies within those packages are processed.
    if let Some(candidates) = all_compatible_installed_pkgs.get(&pkg_type_name) {
        for candidate in candidates {
            if version_req.matches(&candidate.0.version) {
                candidate_pkg_infos.push(candidate.1.clone());
            }
        }
    }

    // Filter suitable candidate packages according to `supports`.
    for mut candidate_pkg_info in candidate_pkg_infos {
        let compatible_score = is_manifest_supports_compatible_with(
            &candidate_pkg_info.manifest.supports.clone().unwrap_or_default(),
            support,
        );

        // A package is considered a candidate only when compatible_score >= 0.
        if compatible_score >= 0 {
            // Record the package's compatible_score so that we can later select
            // the most appropriate one.
            candidate_pkg_info.compatible_score = compatible_score;

            if is_verbose(tman_config.clone()).await {
                out.normal_line(&format!(
                    "=> Found a candidate: {}:{}@{}[{}]",
                    candidate_pkg_info.manifest.type_and_name.pkg_type,
                    candidate_pkg_info.manifest.type_and_name.name.clone(),
                    candidate_pkg_info.manifest.version.clone(),
                    SupportsDisplay(
                        &candidate_pkg_info.manifest.supports.clone().unwrap_or_default(),
                    ),
                ));
            }

            all_candidates
                .entry(pkg_type_name.clone())
                .or_default()
                .insert((&candidate_pkg_info).into(), candidate_pkg_info.clone());

            new_pkgs_to_be_searched.push(candidate_pkg_info);
        }
    }

    Ok(())
}

struct DependenciesContext<'a> {
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    support: &'a ManifestSupport,
    merged_dependencies: &'a mut HashMap<PkgTypeAndName, MergedVersionReq>,
    all_compatible_installed_pkgs: &'a HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    all_candidates: &'a mut HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    new_pkgs_to_be_searched: &'a mut Vec<PkgInfo>,
    pkg_list_cache: &'a mut PackageListCache,
}

/// Asynchronously processes dependencies to get candidate packages.
///
/// # Parameters
///
/// - `all_candidates`: A mutable reference to a map where the keys are package
///   type & name and the values are sets of candidate package information. This
///   map will be updated with potential candidate packages for each dependency.
///
/// # Returns
///
/// A `Result` indicating the success or failure of the operation.
///
/// # Errors
///
/// This function may return an error if there is an issue processing the
/// dependencies.
async fn process_dependencies_to_get_candidates(
    ctx: &mut DependenciesContext<'_>,
    input_dependencies: &Vec<ManifestDependency>,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    for manifest_dep in input_dependencies {
        match manifest_dep {
            ManifestDependency::LocalDependency {
                ..
            } => {
                process_local_dependency_to_get_candidate(
                    manifest_dep,
                    ctx.all_candidates,
                    ctx.new_pkgs_to_be_searched,
                )
                .await?;
            }
            ManifestDependency::RegistryDependency {
                ..
            } => {
                // Check if we need to get the package info from the slow path.
                let changed =
                    merge_dependency_to_dependencies(ctx.merged_dependencies, manifest_dep).await?;
                if !changed {
                    // There is no new information, so we won't proceed further.
                    continue;
                }

                process_non_local_dependency_to_get_candidate(
                    ctx.tman_config.clone(),
                    ctx.support,
                    manifest_dep,
                    ctx.all_compatible_installed_pkgs,
                    ctx.all_candidates,
                    ctx.new_pkgs_to_be_searched,
                    ctx.pkg_list_cache,
                    &out,
                )
                .await?;
            }
        }
    }

    Ok(())
}

/// For a version of a package, only keep the most suitable one. The most
/// suitable one is the package with the highest compatible_score. If there are
/// multiple packages with the highest score, just pick one at random.
fn clean_up_all_candidates(
    all_candidates: &mut HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    locked_pkgs: Option<&HashMap<PkgTypeAndName, PkgInfo>>,
) {
    for (pkg_type_name, pkg_infos) in all_candidates.iter_mut() {
        let mut version_map: HashMap<Version, &PkgInfo> = HashMap::new();
        let mut locked_pkgs_map: HashMap<Version, &PkgInfo> = HashMap::new();

        let locked_pkg = locked_pkgs.and_then(|locked_pkgs| locked_pkgs.get(pkg_type_name));

        for pkg_info in pkg_infos.iter() {
            // Check if the candidate is a locked one.
            if let Some(locked_pkg) = locked_pkg {
                if locked_pkg.manifest.version.clone() == pkg_info.1.manifest.version.clone()
                    && locked_pkg.hash == pkg_info.1.hash
                {
                    locked_pkgs_map.insert(pkg_info.1.manifest.version.clone(), pkg_info.1);
                }
            }

            version_map
                .entry(pkg_info.1.manifest.version.clone())
                .and_modify(|existing_pkg_info| {
                    if pkg_info.1.compatible_score > existing_pkg_info.compatible_score {
                        *existing_pkg_info = pkg_info.1;
                    }
                })
                .or_insert(pkg_info.1);
        }

        // If there is a locked one, replace the candidate with the highest
        // score with the locked one.
        version_map.extend(locked_pkgs_map);

        *pkg_infos =
            version_map.into_values().map(|pkg_info| (pkg_info.into(), pkg_info.clone())).collect();
    }
}

#[allow(clippy::too_many_arguments)]
pub async fn get_all_candidates_from_deps(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    support: &ManifestSupport,
    mut pkgs_to_be_searched: Vec<PkgInfo>,
    extra_dep: Option<&ManifestDependency>,
    all_compatible_installed_pkgs: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    mut all_candidates: HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    locked_pkgs: Option<&HashMap<PkgTypeAndName, PkgInfo>>,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>> {
    let mut merged_dependencies = HashMap::new();
    let mut processed_pkgs = HashSet::<PkgBasicInfo>::new();

    // Cache used to record combinations processed by `get_package_list`.
    let mut pkg_list_cache = PackageListCache::default();

    let mut context = DependenciesContext {
        tman_config,
        support,
        merged_dependencies: &mut merged_dependencies,
        all_compatible_installed_pkgs,
        all_candidates: &mut all_candidates,
        new_pkgs_to_be_searched: &mut Vec::new(),
        pkg_list_cache: &mut pkg_list_cache,
    };

    // If there is extra dependencies (ex: specified in the command line),
    // handle those dependencies, too.
    let mut extra_dep_opt = extra_dep.cloned();

    loop {
        // Merge the dependencies of all pending packages into a single
        // `Vector`.
        let mut combined_dependencies: Vec<ManifestDependency> = Vec::new();

        for pkg_to_be_search in &pkgs_to_be_searched {
            if processed_pkgs.contains(&(pkg_to_be_search).into()) {
                continue;
            }

            // Add all dependencies of the current package to the merged list.
            if let Some(dependencies) = &pkg_to_be_search.manifest.dependencies {
                combined_dependencies.extend(dependencies.clone());
            }

            // Mark the package as processed.
            processed_pkgs.insert(pkg_to_be_search.into());
        }

        // If `extra_dep_opt` exists, add it to `combined_dependencies` (process
        // only once, i.e., the `take()` API).
        if let Some(extra) = extra_dep_opt.take() {
            combined_dependencies.push(extra);
        }

        // Sort the merged `dependencies` by the coverage range of
        // `version_req`, with broader scopes ranked first.
        combined_dependencies.sort_by(|a, b| {
            // Extract version requirements from ManifestDependency enum
            // variants.
            let get_version_req = |dep: &ManifestDependency| -> Option<VersionReq> {
                match dep {
                    ManifestDependency::RegistryDependency {
                        version_req, ..
                    } => Some(version_req.as_processed().clone()),
                    ManifestDependency::LocalDependency {
                        ..
                    } => None,
                }
            };

            let a_req = get_version_req(a);
            let b_req = get_version_req(b);

            match (a_req, b_req) {
                (Some(a_req), Some(b_req)) => {
                    if is_superset_of(&a_req, &b_req) && !is_superset_of(&b_req, &a_req) {
                        std::cmp::Ordering::Less
                    } else if is_superset_of(&b_req, &a_req) && !is_superset_of(&a_req, &b_req) {
                        std::cmp::Ordering::Greater
                    } else {
                        a_req.to_string().cmp(&b_req.to_string())
                    }
                }
                (Some(_), None) => std::cmp::Ordering::Less,
                (None, Some(_)) => std::cmp::Ordering::Greater,
                (None, None) => std::cmp::Ordering::Equal,
            }
        });

        // Process all merged dependencies in a single batch.
        process_dependencies_to_get_candidates(&mut context, &combined_dependencies, out.clone())
            .await?;

        pkgs_to_be_searched.clear();

        if context.new_pkgs_to_be_searched.is_empty() {
            break;
        }
        pkgs_to_be_searched.append(context.new_pkgs_to_be_searched);
    }

    clean_up_all_candidates(&mut all_candidates, locked_pkgs);

    Ok(all_candidates)
}

pub fn get_pkg_info_from_candidates(
    pkg_type: &str,
    pkg_name: &str,
    version: &str,
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
) -> Result<PkgInfo> {
    let pkg_type_name = PkgTypeAndName {
        pkg_type: pkg_type.parse::<PkgType>()?,
        name: pkg_name.to_string(),
    };
    let version_parsed = Version::parse(version)?;
    let pkg_info = all_candidates
        .get(&pkg_type_name)
        .and_then(|set| set.iter().find(|pkg| pkg.1.manifest.version.clone() == version_parsed))
        .ok_or_else(|| anyhow!("PkgInfo not found for [{}]{}@{}", pkg_type, pkg_name, version))?;
    Ok(pkg_info.1.clone())
}
