//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    collections::{HashMap, HashSet},
    sync::Arc,
};

use anyhow::{anyhow, Result};
use clingo::{
    control, Configuration, ConfigurationType, Id, Model, Part, ShowType, SolveMode, Statistics,
    StatisticsType,
};
use semver::Version;
use ten_rust::pkg_info::{
    constants::MANIFEST_JSON_FILENAME, manifest::dependency::ManifestDependency,
    pkg_basic_info::PkgBasicInfo, pkg_type::PkgType, pkg_type_and_name::PkgTypeAndName, PkgInfo,
};

use crate::{
    home::config::{is_verbose, TmanConfig},
    output::TmanOutput,
};
#[derive(Debug)]
pub struct DependencyRelationship {
    pub type_and_name: PkgTypeAndName,
    pub version: Version,
    pub dependency: ManifestDependency,
}

async fn get_model(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    model: &Model,
    is_usable: &mut bool,
    out: Arc<Box<dyn TmanOutput>>,
) -> Option<Vec<String>> {
    // Retrieve the symbols in the model.
    let atoms = model.symbols(ShowType::SHOWN).expect("Failed to retrieve symbols in the model.");

    if is_verbose(tman_config.clone()).await {
        out.normal_line("Model:");
    }

    let mut result = Vec::new();
    *is_usable = true;

    for symbol in atoms {
        if is_verbose(tman_config.clone()).await {
            out.normal_line(&format!(" {symbol}"));
        }

        result.push(symbol.to_string());
        if symbol.to_string().starts_with("error(") {
            *is_usable = false;
        }
    }

    if is_verbose(tman_config.clone()).await {
        out.normal_line("");
    }

    Some(result)
}

#[allow(dead_code)]
async fn print_prefix(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    depth: u8,
    out: Arc<Box<dyn TmanOutput>>,
) {
    if is_verbose(tman_config.clone()).await {
        out.normal_partial("");
    }

    for _ in 0..depth {
        if is_verbose(tman_config.clone()).await {
            out.normal_partial("  ");
        }
    }
}

// Recursively print the configuration object.
#[allow(dead_code)]
async fn print_configuration(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    conf: &Configuration,
    key: Id,
    depth: u8,
    out: Arc<Box<dyn TmanOutput>>,
) {
    let configuration_type = conf.configuration_type(key).unwrap();
    if configuration_type.contains(ConfigurationType::VALUE) {
        let _value = conf.value_get(key).expect("Failed to retrieve statistics value.");
    } else if configuration_type.contains(ConfigurationType::ARRAY) {
        let size = conf.array_size(key).expect("Failed to retrieve statistics array size.");
        for i in 0..size {
            let subkey = conf.array_at(key, i).expect("Failed to retrieve statistics array.");
            print_prefix(tman_config.clone(), depth, out.clone()).await;

            Box::pin(print_configuration(
                tman_config.clone(),
                conf,
                subkey,
                depth + 1,
                out.clone(),
            ))
            .await;
        }
    } else if configuration_type.contains(ConfigurationType::MAP) {
        let size = conf.map_size(key).unwrap();
        for i in 0..size {
            let name = conf.map_subkey_name(key, i).unwrap();
            let subkey = conf.map_at(key, name).unwrap();
            print_prefix(tman_config.clone(), depth, out.clone()).await;

            Box::pin(print_configuration(
                tman_config.clone(),
                conf,
                subkey,
                depth + 1,
                out.clone(),
            ))
            .await;
        }
    } else {
        unreachable!()
    }
}

// recursively print the statistics object
#[allow(dead_code)]
async fn print_statistics(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    stats: &Statistics,
    key: u64,
    depth: u8,
    out: Arc<Box<dyn TmanOutput>>,
) {
    // Get the type of an entry and switch over its various values.
    let statistics_type = stats.statistics_type(key).unwrap();
    match statistics_type {
        StatisticsType::Value => {
            let value = stats.value_get(key).expect("Failed to retrieve statistics value.");

            out.normal_line(&format!(" {value}"));
        }

        StatisticsType::Array => {
            let size = stats.array_size(key).expect("Failed to retrieve statistics array size.");
            for i in 0..size {
                let subkey = stats.array_at(key, i).expect("Failed to retrieve statistics array.");
                print_prefix(tman_config.clone(), depth, out.clone()).await;
                out.normal_partial(&format!("{i} zu:"));

                Box::pin(print_statistics(
                    tman_config.clone(),
                    stats,
                    subkey,
                    depth + 1,
                    out.clone(),
                ))
                .await;
            }
        }

        StatisticsType::Map => {
            let size = stats.map_size(key).unwrap();
            for i in 0..size {
                let name = stats.map_subkey_name(key, i).unwrap();
                let subkey = stats.map_at(key, name).unwrap();
                print_prefix(tman_config.clone(), depth, out.clone()).await;
                out.normal_partial(&format!("{name}:"));

                Box::pin(print_statistics(
                    tman_config.clone(),
                    stats,
                    subkey,
                    depth + 1,
                    out.clone(),
                ))
                .await;
            }
        }

        StatisticsType::Empty => {
            out.normal_line("StatisticsType::Empty");
        }
    }
}

type UsableModel = Option<Vec<String>>;
type NonUsableModels = Vec<Vec<String>>;
type SolveOutcome = (UsableModel, NonUsableModels);
type SolveResult = Result<SolveOutcome>;

#[allow(unused_assignments)]
async fn solve(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    input: &str,
    out: Arc<Box<dyn TmanOutput>>,
) -> SolveResult {
    // Create a control object.
    // i.e., clingo_control_new
    let mut ctl = control({
        let mut args = vec![];

        if is_verbose(tman_config.clone()).await {
            args.push("--verbose".to_string());
        }

        args
    })
    .expect("Failed creating Control.");

    {
        // Get the configuration object and its root key.
        let conf = ctl.configuration_mut().unwrap();
        let root_key = conf.root().unwrap();

        // Configure to enumerate all models.
        let solve_models_key = conf.map_at(root_key, "solve.models").unwrap();
        conf.value_set(solve_models_key, "0").expect("Failed to set solve.models to 0.");

        // Configure to enumerate all optimal models.
        let solve_opt_mode_key = conf.map_at(root_key, "solve.opt_mode").unwrap();
        conf.value_set(solve_opt_mode_key, "optN").expect("Failed to set solve.opt_mode to optN.");

        // Enable full statistics.
        let stats_key = conf.map_at(root_key, "stats").unwrap();
        conf.value_set(stats_key, "2").expect("Failed to set stats_key to 2.");

        // Configure the first solver to use the berkmin heuristic.
        let mut solver_key = conf.map_at(root_key, "solver").unwrap();
        solver_key = conf.array_at(solver_key, 0).unwrap();
        let heuristic_key = conf.map_at(solver_key, "heuristic").unwrap();
        conf.value_set(heuristic_key, "berkmin").expect("Failed to set heuristic to berkmin.");

        // print_configuration(
        //     tman_config.clone(),
        //     conf,
        //     root_key,
        //     0,
        //     out.clone(),
        // )
        // .await;
    }

    let main_program = include_str!("main.lp");
    let display_program = include_str!("display.lp");

    // Add a logic program to the base part.
    // i.e., clingo_control_add
    ctl.add("main", &[], main_program).expect("Failed to add main.lp");
    ctl.add("display", &[], display_program).expect("Failed to add display.lp");
    ctl.add("base", &[], input).expect("Failed to add input.lp");

    // Ground the parts.
    // i.e., clingo_control_ground
    let main_part = Part::new("main", vec![]).unwrap();
    let display_part = Part::new("display", vec![]).unwrap();
    let base_part = Part::new("base", vec![]).unwrap();

    let parts = vec![main_part, display_part, base_part];
    ctl.ground(&parts).expect("Failed to ground a logic program.");

    // Solving. Get a solve handle.
    // i.e., clingo_control_solve
    let mut handle = ctl.solve(SolveMode::YIELD, &[]).expect("Failed retrieving solve handle.");

    let mut usable_model = None;
    let mut non_usable_models = Vec::new();

    // Loop over all models.
    loop {
        // i.e., clingo_solve_handle_resume
        handle.resume().expect("Failed resume on solve handle.");

        // i.e., clingo_solve_handle_model
        match handle.model() {
            // Get the model.
            Ok(Some(model)) => {
                let mut is_usable = false;
                if let Some(m) =
                    get_model(tman_config.clone(), model, &mut is_usable, out.clone()).await
                {
                    if is_usable {
                        usable_model = Some(m);

                        // Clingo will only output increasingly optimized
                        // models, so we need to take the last model without an
                        // error statement from all outputted models, not just
                        // the first good one. Therefore, we shouldn't break
                        // here.
                    } else {
                        non_usable_models.push(m); // Collect error models.
                    }
                }
            }
            // Stop if there are no more models.
            Ok(None) => {
                if is_verbose(tman_config.clone()).await {
                    out.normal_line("No more models");
                }
                break;
            }
            Err(e) => panic!("Error: {e}"),
        }
    }

    // Close the solve handle.
    // i.e., clingo_solve_handle_get
    let _result = handle.get().expect("Failed to get result from solve handle.");

    // Free the solve handle.
    // i.e., clingo_solve_handle_close
    ctl = handle.close().expect("Failed to close solve handle.");

    // Get the statistics object, get the root key, then print the statistics
    // recursively.
    // let stats = ctl.statistics().unwrap();
    // let stats_key = stats.root().unwrap();
    // print_statistics(tman_config.clone(), stats, stats_key, 0, out.clone())
    //     .await;

    Ok((usable_model, non_usable_models))
}

async fn create_input_str_for_dependency_relationship(
    input_str: &mut String,
    dep_relationship: Option<&DependencyRelationship>,
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
) -> Result<()> {
    if let Some(dep_relationship) = dep_relationship {
        let pkg_type_and_name = match &dep_relationship.dependency {
            ManifestDependency::RegistryDependency {
                pkg_type,
                name,
                ..
            } => PkgTypeAndName {
                pkg_type: *pkg_type,
                name: name.clone(),
            },
            ManifestDependency::LocalDependency {
                path,
                base_dir,
                ..
            } => {
                // Get type and name from the manifest.
                let base_dir_str = base_dir.as_deref().ok_or_else(|| {
                    anyhow!("base_dir cannot be None when processing local dependency")
                })?;
                let abs_path = std::path::Path::new(base_dir_str).join(path);
                let dep_manifest_path = abs_path.join(MANIFEST_JSON_FILENAME);

                // Parse manifest to get type and name.
                let manifest =
                    ten_rust::pkg_info::manifest::parse_manifest_from_file(&dep_manifest_path)
                        .await?;
                manifest.type_and_name
            }
        };

        let candidates = all_candidates.get(&pkg_type_and_name);

        if let Some(candidates) = candidates {
            for candidate in candidates.iter() {
                // Get version requirement from dependency.
                let version_matches = match &dep_relationship.dependency {
                    ManifestDependency::RegistryDependency {
                        version_req, ..
                    } => version_req.matches(&candidate.1.manifest.version),
                    ManifestDependency::LocalDependency {
                        ..
                    } => {
                        // For local dependencies, just return true to match all
                        // versions.
                        true
                    }
                };

                if version_matches {
                    input_str.push_str(&format!(
                        "depends_on_declared(\"{}\", \"{}\", \"{}\", \"{}\", \"{}\", \"{}\").\n",
                        dep_relationship.type_and_name.pkg_type,
                        dep_relationship.type_and_name.name,
                        dep_relationship.version,
                        candidate.1.manifest.type_and_name.pkg_type,
                        candidate.1.manifest.type_and_name.name,
                        candidate.1.manifest.version,
                    ));
                }
            }
        } else {
            return Err(anyhow!(
                "Failed to find candidates for {}",
                match &dep_relationship.dependency {
                    ManifestDependency::RegistryDependency {
                        pkg_type,
                        name,
                        ..
                    } => format!("{}:{} @ {}", pkg_type, name, dep_relationship.version),
                    ManifestDependency::LocalDependency {
                        path, ..
                    } => format!("local:{} @ {}", path, dep_relationship.version),
                }
            ));
        }
    }

    Ok(())
}

async fn create_input_str_for_pkg_info_dependencies(
    input_str: &mut String,
    pkg_info: &PkgInfo,
    dumped_pkgs_info: &mut HashSet<PkgBasicInfo>,
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    max_latest_versions: i32,
) -> Result<()> {
    // If this package has already been dumped, skip it.
    if dumped_pkgs_info.contains(&pkg_info.into()) {
        return Ok(());
    }
    dumped_pkgs_info.insert(pkg_info.into());

    // Get dependencies from the manifest

    if let Some(dependencies) = &pkg_info.manifest.dependencies {
        for dependency in dependencies {
            let pkg_type_and_name = match dependency {
                ManifestDependency::RegistryDependency {
                    pkg_type,
                    name,
                    ..
                } => PkgTypeAndName {
                    pkg_type: *pkg_type,
                    name: name.clone(),
                },
                ManifestDependency::LocalDependency {
                    path,
                    base_dir,
                    ..
                } => {
                    // Get type and name from the manifest.
                    let base_dir_str = base_dir.as_deref().ok_or_else(|| {
                        anyhow!("base_dir cannot be None when processing local dependency")
                    })?;
                    let abs_path = std::path::Path::new(base_dir_str).join(path);
                    let dep_manifest_path = abs_path.join(MANIFEST_JSON_FILENAME);

                    // Parse manifest to get type and name.
                    let manifest =
                        ten_rust::pkg_info::manifest::parse_manifest_from_file(&dep_manifest_path)
                            .await?;
                    manifest.type_and_name
                }
            };

            let candidates = all_candidates.get(&pkg_type_and_name);

            if let Some(candidates) = candidates {
                let mut candidates_vec: Vec<&PkgInfo> = candidates.values().collect();

                // The sorting below places the larger versions at the front,
                // thus having smaller indexes. This is correct because, in the
                // Clingo solver, our optimization strategy is to minimize the
                // overall weight, and we prefer larger version numbers.
                // Therefore, larger version numbers have smaller weights, and
                // the index here is equivalent to the concept of weight in the
                // Clingo solver.
                candidates_vec.sort_by(|a, b| b.manifest.version.cmp(&a.manifest.version));

                let mut found_matched_count = 0;
                let mut found_any_valid_dep = false;

                for candidate in candidates_vec.into_iter() {
                    // Get version requirement from dependency.
                    let version_matches = match dependency {
                        ManifestDependency::RegistryDependency {
                            version_req, ..
                        } => version_req.matches(&candidate.manifest.version),
                        ManifestDependency::LocalDependency {
                            ..
                        } => {
                            // For local dependencies, just return true to
                            // match all versions.
                            true
                        }
                    };

                    if version_matches {
                        found_matched_count += 1;

                        // Before adding this package into the computation of the result,
                        // first check whether all of this package's dependencies are valid.
                        // For example, some packages that this one depends on might not exist
                        // in the cloud marketplace or the local file system.
                        // Including such a package — one that cannot actually be used in the
                        // computation — would cause problems and make no
                        // sense.
                        let dep_result = Box::pin(create_input_str_for_pkg_info_dependencies(
                            input_str,
                            candidate,
                            dumped_pkgs_info,
                            all_candidates,
                            max_latest_versions,
                        ))
                        .await;

                        // Only add the dependency declaration if the candidate's dependencies are
                        // valid
                        if dep_result.is_ok() {
                            input_str.push_str(&format!(
                                "depends_on_declared(\"{}\", \"{}\", \"{}\", \"{}\", \"{}\", \
                                 \"{}\").\n",
                                pkg_info.manifest.type_and_name.pkg_type,
                                pkg_info.manifest.type_and_name.name,
                                pkg_info.manifest.version,
                                candidate.manifest.type_and_name.pkg_type,
                                candidate.manifest.type_and_name.name,
                                candidate.manifest.version,
                            ));
                            found_any_valid_dep = true;
                        }

                        if found_matched_count >= max_latest_versions {
                            break;
                        }
                    }
                }

                if found_matched_count == 0 {
                    // No versions match the requirement - return error
                    return Err(anyhow!(
                        "No matching versions found for dependency {}:{}",
                        pkg_type_and_name.pkg_type,
                        pkg_type_and_name.name
                    ));
                }

                if !found_any_valid_dep {
                    // We found matching versions, but none of them have valid dependencies
                    return Err(anyhow!(
                        "No valid dependencies found for {}:{}",
                        pkg_type_and_name.pkg_type,
                        pkg_type_and_name.name
                    ));
                }
            } else {
                // No candidates found for this dependency - return error
                return Err(anyhow!(
                    "No candidates found for dependency {}:{}",
                    pkg_type_and_name.pkg_type,
                    pkg_type_and_name.name
                ));
            }
        }
    }

    // All dependencies are valid
    Ok(())
}

fn create_input_str_for_pkg_info_without_dependencies(
    input_str: &mut String,
    pkg_info: &PkgInfo,
    weight: &usize,
) -> Result<()> {
    input_str.push_str(&format!(
        "version_declared(\"{}\", \"{}\", \"{}\", {}).\n",
        pkg_info.manifest.type_and_name.pkg_type,
        pkg_info.manifest.type_and_name.name,
        pkg_info.manifest.version,
        weight
    ));

    Ok(())
}

fn create_input_str_for_all_possible_pkgs_info_with_filter(
    input_str: &mut String,
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    locked_pkgs: Option<&HashMap<PkgTypeAndName, PkgInfo>>,
    max_latest_versions: i32,
    valid_packages: Option<&HashSet<PkgBasicInfo>>,
) -> Result<()> {
    for candidates in all_candidates {
        let mut candidates_vec: Vec<&PkgInfo> = candidates.1.values().collect();

        // The sorting below places the larger versions at the front, thus
        // having smaller indexes. This is correct because, in the Clingo
        // solver, our optimization strategy is to minimize the overall weight,
        // and we prefer larger version numbers. Therefore, larger version
        // numbers have smaller weights, and the index here is equivalent to the
        // concept of weight in the Clingo solver.
        candidates_vec.sort_by(|a, b| b.manifest.version.cmp(&a.manifest.version));

        // Check if the locked package exists in the candidates. If it does,
        // move it to the front of the candidates_vec so that it has a smaller
        // weight.
        let locked_pkg = locked_pkgs.and_then(|locked_pkgs| locked_pkgs.get(candidates.0));

        if let Some(locked_pkg) = locked_pkg {
            // If the package recorded in `manifest-lock.json` is a local
            // dependency, do not prioritize any candidate packages.
            if !locked_pkg.is_local_dependency {
                let idx = candidates_vec
                    .iter()
                    .position(|pkg_info| locked_pkg.manifest.version == pkg_info.manifest.version);

                if let Some(idx) = idx {
                    candidates_vec.remove(idx);
                    candidates_vec.insert(0, locked_pkg);
                }
            }
        }

        for (idx, candidate) in candidates_vec.into_iter().enumerate() {
            if max_latest_versions >= 0 && idx >= max_latest_versions as usize {
                break;
            }

            // If we're using a filter, only add version_declared for valid packages
            if let Some(valid_pkgs) = valid_packages {
                let pkg_basic_info: PkgBasicInfo = candidate.into();
                if !valid_pkgs.contains(&pkg_basic_info) {
                    continue;
                }
            }

            create_input_str_for_pkg_info_without_dependencies(input_str, candidate, &idx)?;
        }
    }

    Ok(())
}

#[allow(clippy::too_many_arguments)]
async fn create_input_str(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pkg_type: &PkgType,
    pkg_name: &String,
    extra_dep_relationship: Option<&DependencyRelationship>,
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    locked_pkgs: Option<&HashMap<PkgTypeAndName, PkgInfo>>,
    out: Arc<Box<dyn TmanOutput>>,
    max_latest_versions: i32,
) -> Result<String> {
    let mut input_str = String::new();

    input_str.push_str(&format!("root_declared(\"{pkg_type}\", \"{pkg_name}\").\n",));

    create_input_str_for_dependency_relationship(
        &mut input_str,
        extra_dep_relationship,
        all_candidates,
    )
    .await?;

    // First pass: determine which package versions have valid dependencies
    let mut dumped_pkgs_info = HashSet::new();
    let mut skipped_packages = Vec::new();
    let mut valid_packages: HashSet<PkgBasicInfo> = HashSet::new();

    for candidates in all_candidates {
        for candidate in candidates.1 {
            let result = create_input_str_for_pkg_info_dependencies(
                &mut input_str,
                candidate.1,
                &mut dumped_pkgs_info,
                all_candidates,
                max_latest_versions,
            )
            .await;

            if result.is_err() {
                // Skip this candidate package if its dependencies cannot be satisfied
                skipped_packages.push(format!(
                    "{}:{}@{}",
                    candidate.1.manifest.type_and_name.pkg_type,
                    candidate.1.manifest.type_and_name.name,
                    candidate.1.manifest.version
                ));
            } else {
                // Mark this version as valid
                valid_packages.insert(candidate.1.into());
            }
        }
    }

    // Second pass: only add version_declared for valid package versions
    create_input_str_for_all_possible_pkgs_info_with_filter(
        &mut input_str,
        all_candidates,
        locked_pkgs,
        max_latest_versions,
        Some(&valid_packages),
    )?;

    if is_verbose(tman_config.clone()).await {
        if !skipped_packages.is_empty() {
            out.normal_line(&format!(
                "Skipped {} candidate package(s) due to unsatisfiable dependencies:",
                skipped_packages.len()
            ));
            for pkg in &skipped_packages {
                out.normal_line(&format!("  - {}", pkg));
            }
        }
        out.normal_line(&format!("Input: \n{input_str}"));
    }

    Ok(input_str)
}

#[allow(clippy::too_many_arguments)]
pub async fn solve_all(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pkg_type: &PkgType,
    pkg_name: &String,
    extra_dep_relationship: Option<&DependencyRelationship>,
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    locked_pkgs: Option<&HashMap<PkgTypeAndName, PkgInfo>>,
    out: Arc<Box<dyn TmanOutput>>,
    max_latest_versions: i32,
) -> SolveResult {
    let input_str = create_input_str(
        tman_config.clone(),
        pkg_type,
        pkg_name,
        extra_dep_relationship,
        all_candidates,
        locked_pkgs,
        out.clone(),
        max_latest_versions,
    )
    .await?;
    solve(tman_config, &input_str, out).await
}
