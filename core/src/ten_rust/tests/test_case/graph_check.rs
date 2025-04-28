//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, path::Path, str::FromStr};

use uuid::Uuid;

use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp,
    graph::{graph_info::GraphInfo, Graph},
    pkg_info::get_app_installed_pkgs,
};

#[test]
fn test_graph_check_extension_not_installed_1() {
    let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

    let app_dir = "tests/test_data/graph_check_extension_not_installed_1";
    let pkgs_info_in_app = get_app_installed_pkgs(
        Path::new(app_dir),
        true,
        &mut Some(&mut graphs_cache),
    )
    .unwrap();
    assert!(!pkgs_info_in_app.is_empty());

    let (_, graph_info) = graphs_cache.into_iter().next().unwrap();

    let graph = &graph_info.graph;

    let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
    pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

    let result = graph.check(&Some(app_dir.to_string()), &pkgs_cache);
    assert!(result.is_err());
    println!("Error: {:?}", result.err().unwrap());
}

#[test]
fn test_graph_check_extension_not_installed_2() {
    let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

    let app_dir = "tests/test_data/graph_check_extension_not_installed_2";
    let pkgs_info_in_app = get_app_installed_pkgs(
        Path::new(app_dir),
        true,
        &mut Some(&mut graphs_cache),
    )
    .unwrap();
    assert!(!pkgs_info_in_app.is_empty());

    let (_, graph_info) = graphs_cache.into_iter().next().unwrap();
    let graph = &graph_info.graph;

    let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
    pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

    let result = graph.check(&Some(app_dir.to_string()), &pkgs_cache);
    assert!(result.is_err());
    println!("Error: {:?}", result.err().unwrap());
}

#[test]
fn test_graph_check_predefined_graph_success() {
    let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

    let app_dir = "tests/test_data/graph_check_predefined_graph_success";
    let pkgs_info_in_app = get_app_installed_pkgs(
        Path::new(app_dir),
        true,
        &mut Some(&mut graphs_cache),
    )
    .unwrap();
    assert!(!pkgs_info_in_app.is_empty());

    let (_, graph_info) = graphs_cache.into_iter().next().unwrap();
    let graph = &graph_info.graph;

    let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
    pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

    let result = graph.check(&Some(app_dir.to_string()), &pkgs_cache);
    eprintln!("result: {:?}", result);
    assert!(result.is_ok());
}

#[test]
fn test_graph_check_all_msgs_schema_incompatible() {
    let mut graphs_cache: HashMap<Uuid, GraphInfo> = HashMap::new();

    let app_dir = "tests/test_data/graph_check_all_msgs_schema_incompatible";
    let pkgs_info_in_app = get_app_installed_pkgs(
        Path::new(app_dir),
        true,
        &mut Some(&mut graphs_cache),
    )
    .unwrap();
    assert!(!pkgs_info_in_app.is_empty());

    let (_, graph_info) = graphs_cache.into_iter().next().unwrap();
    let graph = &graph_info.graph;

    let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
    pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

    let result = graph.check(&Some(app_dir.to_string()), &pkgs_cache);
    assert!(result.is_err());
    println!("Error: {:?}", result.err().unwrap());
}

#[test]
fn test_graph_check_single_app() {
    let app_dir = "tests/test_data/graph_check_single_app";
    let pkgs_info_in_app = get_app_installed_pkgs(
        Path::new(app_dir),
        true,
        &mut Some(&mut HashMap::new()),
    )
    .unwrap();
    assert!(!pkgs_info_in_app.is_empty());

    let graph_json_str =
        include_str!("../test_data/graph_check_single_app/graph.json");
    let graph = Graph::from_str(graph_json_str).unwrap();

    let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
    pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

    // The schema of 'ext_c' is not found, but it's OK because we only check
    // for the app 'http://localhost:8001'.
    let result =
        graph.check_for_single_app(&Some(app_dir.to_string()), &pkgs_cache);
    eprintln!("result: {:?}", result);
    assert!(result.is_ok());
}

#[test]
fn test_graph_check_builtin_extension() {
    let app_dir = "tests/test_data/graph_check_builtin_extension";
    let pkgs_info_in_app = get_app_installed_pkgs(
        Path::new(app_dir),
        true,
        &mut Some(&mut HashMap::new()),
    )
    .unwrap();
    assert!(!pkgs_info_in_app.is_empty());

    let graph_json_str =
        include_str!("../test_data/graph_check_builtin_extension/graph.json");
    let graph = Graph::from_str(graph_json_str).unwrap();

    let mut pkgs_cache: HashMap<String, PkgsInfoInApp> = HashMap::new();
    pkgs_cache.insert(app_dir.to_string(), pkgs_info_in_app);

    let result =
        graph.check_for_single_app(&Some(app_dir.to_string()), &pkgs_cache);
    eprintln!("result: {:?}", result);
    assert!(result.is_ok());
}
