//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use anyhow::{anyhow, Result};
use console::Emoji;
use regex::Regex;

use ten_rust::pkg_info::{
    pkg_basic_info::PkgBasicInfo, pkg_type_and_name::PkgTypeAndName, PkgInfo,
};

use crate::{
    dep_and_candidate::get_pkg_info_from_candidates, output::TmanOutput,
    solver::introducer::get_dependency_chain,
};

#[derive(Debug)]
pub struct ConflictPkgVersion {
    // The version of the pkg that causes the conflict, introduced by the
    // following introducer.
    pub pkg_version: String,

    pub introducer_type: String,
    pub introducer_name: String,
    pub introducer_version: String,
}

#[derive(Debug)]
pub struct ConflictInfo {
    pub conflict_pkg_type: String,
    pub conflict_pkg_name: String,

    // The introducers are needed to get the dependency chain. The conflicted
    // pkg (i.e., pkg_type and pkg_name) can not be used to get the dependency
    // chain, considering the following scenario:
    //
    // Pkg A depends on D@0.1.0, B depends on D@0.2.0, and C depends on
    // D@0.2.1. Then clingo will give us two answer models with errors:
    //
    // - ... @0.1.0 introduced by A, and @0.2.0 introduced by B ...
    // - ... @0.1.0 introduced by A, and @0.2.1 introduced by C ...
    //
    // If we choose the first error message as the report, and try to get the
    // dependency chain using the conflicted pkg (i.e., D), we might get the
    // wrong chain, ex:
    //
    // └─ [extension]A
    //    └─ [extension]D@0.1.0
    //
    // └─ [extension]C
    //    └─ [extension]D@0.2.1
    //
    // But what we want is the chain leading to D@0.2.0.
    pub conflict_pkg_version_1: ConflictPkgVersion,
    pub conflict_pkg_version_2: ConflictPkgVersion,

    pub error_message: String,
}

pub fn parse_error_statement(clingo_output: &[String]) -> Result<ConflictInfo> {
    // Regular expression to match the error statement.
    let error_re = Regex::new(
        r#"error\(\s*\d+,\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\)"#,
    )?;

    for line in clingo_output {
        if let Some(caps) = error_re.captures(line) {
            let error_template = caps.get(1).unwrap().as_str();
            let pkg_type = caps.get(2).unwrap().as_str().to_string();
            let pkg_name = caps.get(3).unwrap().as_str().to_string();
            let version1 = caps.get(4).unwrap().as_str().to_string();
            let version2 = caps.get(5).unwrap().as_str().to_string();
            let introducer1_type = caps.get(6).unwrap().as_str().to_string();
            let introducer1_name = caps.get(7).unwrap().as_str().to_string();
            let introducer1_version = caps.get(8).unwrap().as_str().to_string();
            let introducer2_type = caps.get(9).unwrap().as_str().to_string();
            let introducer2_name = caps.get(10).unwrap().as_str().to_string();
            let introducer2_version =
                caps.get(11).unwrap().as_str().to_string();

            // Format the error message with the extracted arguments.
            let error_message = error_template
                .replace("{0}", &pkg_type)
                .replace("{1}", &pkg_name)
                .replace("{2}", &version1)
                .replace("{3}", &version2)
                .replace("{4}", &introducer1_type)
                .replace("{5}", &introducer1_name)
                .replace("{6}", &introducer1_version)
                .replace("{7}", &introducer2_type)
                .replace("{8}", &introducer2_name)
                .replace("{9}", &introducer2_version);

            return Ok(ConflictInfo {
                conflict_pkg_type: pkg_type,
                conflict_pkg_name: pkg_name,

                conflict_pkg_version_1: ConflictPkgVersion {
                    introducer_type: introducer1_type,
                    introducer_name: introducer1_name,
                    introducer_version: introducer1_version,
                    pkg_version: version1,
                },
                conflict_pkg_version_2: ConflictPkgVersion {
                    introducer_type: introducer2_type,
                    introducer_name: introducer2_name,
                    introducer_version: introducer2_version,
                    pkg_version: version2,
                },

                error_message,
            });
        }
    }

    Err(anyhow!("No error message found in the Clingo output."))
}

fn print_dependency_chain(
    chain: &[(String, PkgTypeAndName)],
    pkg_type: &str,
    pkg_name: &str,
    version: &str,
    out: Arc<Box<dyn TmanOutput>>,
) {
    out.normal_line(&format!(
        "Dependency chain leading to [{}]{}@{}:",
        pkg_type, pkg_name, version
    ));
    for (i, pkg) in chain.iter().enumerate() {
        out.normal_line(&format!(
            "{:indent$}└─ [{}]{}@{}",
            "",
            pkg.1.pkg_type,
            pkg.1.name,
            pkg.0,
            indent = i * 3
        ));
    }
    out.normal_line("");
}

pub fn print_conflict_info(
    conflict_info: &ConflictInfo,
    introducer_relations: &HashMap<PkgBasicInfo, (String, Option<PkgInfo>)>,
    all_candidates: &HashMap<PkgTypeAndName, HashMap<PkgBasicInfo, PkgInfo>>,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    out.normal_line(&format!(
        "{}  Error: {}",
        Emoji("🔴", ":-("),
        conflict_info.error_message
    ));
    out.normal_line("");

    // Get PkgInfo for both introducer packages.
    let introducer_pkg_info_1 = get_pkg_info_from_candidates(
        &conflict_info.conflict_pkg_version_1.introducer_type,
        &conflict_info.conflict_pkg_version_1.introducer_name,
        &conflict_info.conflict_pkg_version_1.introducer_version,
        all_candidates,
    )?;
    let introducer_pkg_info_2 = get_pkg_info_from_candidates(
        &conflict_info.conflict_pkg_version_2.introducer_type,
        &conflict_info.conflict_pkg_version_2.introducer_name,
        &conflict_info.conflict_pkg_version_2.introducer_version,
        all_candidates,
    )?;

    // The introducer_pkg_info_1/introducer_pkg_info_2 (i.e., the introducer)
    // depends on the pkg identified by `conflict_pkg_identity`.
    let conflict_pkg_identity = PkgTypeAndName {
        pkg_type: conflict_info.conflict_pkg_type.parse()?,
        name: conflict_info.conflict_pkg_name.clone(),
    };

    // Get dependency chains.
    let chain1 = get_dependency_chain(
        &introducer_pkg_info_1,
        &conflict_pkg_identity,
        introducer_relations,
    );
    let chain2 = get_dependency_chain(
        &introducer_pkg_info_2,
        &conflict_pkg_identity,
        introducer_relations,
    );

    print_dependency_chain(
        &chain1,
        &conflict_info.conflict_pkg_type,
        &conflict_info.conflict_pkg_name,
        &conflict_info.conflict_pkg_version_1.pkg_version,
        out.clone(),
    );

    print_dependency_chain(
        &chain2,
        &conflict_info.conflict_pkg_type,
        &conflict_info.conflict_pkg_name,
        &conflict_info.conflict_pkg_version_2.pkg_version,
        out.clone(),
    );

    Ok(())
}
