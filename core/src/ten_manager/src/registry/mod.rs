//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod found_result;
pub mod local;
mod pkg_cache;
pub mod pkg_list_cache;
mod remote;
pub mod search;

use std::sync::Arc;

use anyhow::{anyhow, Result};
use found_result::PkgRegistryInfo;
use semver::{Version, VersionReq};
use tempfile::NamedTempFile;
use ten_rust::pkg_info::{pkg_type::PkgType, PkgInfo};

use super::{constants::DEFAULT, home::config::TmanConfig};
use crate::{output::TmanOutput, registry::search::PkgSearchFilter};

pub async fn upload_package(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    package_file_path: &str,
    pkg_info: &PkgInfo,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<String> {
    let default_registry_url = tman_config
        .read()
        .await
        .registry
        .get(DEFAULT)
        .ok_or_else(|| anyhow!("Default registry not found"))?
        .index
        .clone();

    let parsed_registry_url = match url::Url::parse(&default_registry_url) {
        Ok(url) => url,
        Err(_) => return Err(anyhow!("Invalid URL: {}", default_registry_url)),
    };

    match parsed_registry_url.scheme() {
        "file" => {
            local::upload_package(&default_registry_url, package_file_path, pkg_info, out).await
        }
        "https" => {
            remote::upload_package(
                tman_config,
                &default_registry_url,
                package_file_path,
                pkg_info,
                out,
            )
            .await
        }
        _ => Err(anyhow!("Unrecognized URL scheme: {}", parsed_registry_url.scheme())),
    }
}

pub async fn get_package(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pkg_type: &PkgType,
    pkg_name: &str,
    pkg_version: &Version,
    url: &str,
    temp_path: &mut NamedTempFile,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    let parsed_url = url::Url::parse(url).map_err(|_| anyhow!("Invalid URL: {}", url))?;

    match parsed_url.scheme() {
        "file" => {
            local::get_package(tman_config, pkg_type, pkg_name, pkg_version, url, temp_path, out)
                .await
        }
        "https" => {
            remote::get_package(tman_config, pkg_type, pkg_name, pkg_version, url, temp_path, out)
                .await
        }
        _ => Err(anyhow!("Failed to get package to any configured registry.")),
    }
}

/// Retrieves a list of packages from the registry that match the specified
/// criteria.
///
/// # Arguments
/// * `tman_config` - Configuration containing registry information.
/// * `pkg_type` - Optional type of package to search for (e.g., app,
///   extension).
/// * `name` - Optional name of the package to search for.
/// * `version_req` - Optional version requirement to filter packages.
/// * `tags` - Optional tags to filter packages by. If specified, only packages
///   with all the specified tags will be returned.
/// * `page_size` - Optional number of items per page. Default is 100 if not
///   specified.
/// * `page` - Optional page number to retrieve. If not specified, all items are
///   retrieved.
/// * `out` - Output interface for logging.
///
/// # Returns
/// A vector of `PkgRegistryInfo` containing information about matching
/// packages.
///
/// # Errors
/// * If the default registry is not configured.
/// * If the registry URL is invalid.
/// * If the URL scheme is not supported (only "file" and "https" are
///   supported).
/// * If there's an error retrieving the package list from the registry.
#[allow(clippy::too_many_arguments)]
pub async fn get_package_list(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pkg_type: Option<PkgType>,
    name: Option<String>,
    version_req: Option<VersionReq>,
    tags: Option<Vec<String>>,
    scope: Option<Vec<String>>,
    page_size: Option<u32>,
    page: Option<u32>,
    out: &Arc<Box<dyn TmanOutput>>,
) -> Result<Vec<PkgRegistryInfo>> {
    // Retrieve the default registry URL from configuration.
    let default_registry_url = tman_config
        .read()
        .await
        .registry
        .get(DEFAULT)
        .ok_or_else(|| anyhow!("Default registry not found"))?
        .index
        .clone();

    // Parse the registry URL to determine the scheme (file or https).
    let parsed_registry_url = url::Url::parse(&default_registry_url)
        .map_err(|_| anyhow!("Invalid URL: {}", default_registry_url))?;

    // Delegate to the appropriate handler based on the URL scheme.
    let results = match parsed_registry_url.scheme() {
        "file" => {
            local::get_package_list(
                tman_config,
                &default_registry_url,
                pkg_type,
                name,
                version_req,
                tags,
                scope,
                page_size,
                page,
                out,
            )
            .await?
        }
        "https" => {
            remote::get_package_list(
                tman_config,
                &default_registry_url,
                pkg_type,
                name,
                version_req,
                tags,
                scope,
                page_size,
                page,
                out,
            )
            .await?
        }
        _ => {
            return Err(anyhow!("Unsupported URL scheme: {}", parsed_registry_url.scheme()));
        }
    };

    // Sort packages by version in descending order (newer versions first).
    let mut sorted_results = results;
    sorted_results.sort_by(|a, b| b.basic_info.version.cmp(&a.basic_info.version));

    Ok(sorted_results)
}

#[allow(clippy::too_many_arguments)]
pub async fn search_packages(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    filter: &PkgSearchFilter,
    page_size: Option<u32>,
    page: Option<u32>,
    sort_by: Option<&str>,
    sort_order: Option<&str>,
    scope: Option<&str>,
    out: &Arc<Box<dyn TmanOutput>>,
) -> Result<(u32, Vec<PkgRegistryInfo>)> {
    // Retrieve the default registry URL from configuration.
    let default_registry_url = tman_config
        .read()
        .await
        .registry
        .get(DEFAULT)
        .ok_or_else(|| anyhow!("Default registry not found"))?
        .index
        .clone();

    // Parse the registry URL to determine the scheme (file or https).
    let parsed_registry_url = url::Url::parse(&default_registry_url)
        .map_err(|_| anyhow!("Invalid URL: {}", default_registry_url))?;

    // Delegate to the appropriate handler based on the URL scheme.
    let results = match parsed_registry_url.scheme() {
        "file" => {
            local::search_packages(
                tman_config.clone(),
                &default_registry_url,
                filter,
                page_size,
                page,
                sort_by,
                sort_order,
                scope,
                out,
            )
            .await?
        }
        "https" => {
            remote::search_packages(
                tman_config.clone(),
                &default_registry_url,
                filter,
                page_size,
                page,
                sort_by,
                sort_order,
                scope,
                out,
            )
            .await?
        }
        _ => {
            return Err(anyhow!("Unsupported URL scheme: {}", parsed_registry_url.scheme()));
        }
    };
    Ok(results)
}

pub async fn delete_package(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pkg_type: PkgType,
    name: &String,
    version: &Version,
    hash: &String,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    // Retrieve the default registry URL.
    let default_registry_url = tman_config
        .read()
        .await
        .registry
        .get(DEFAULT)
        .ok_or_else(|| anyhow!("Default registry not found"))?
        .index
        .clone();

    let parsed_registry_url = url::Url::parse(&default_registry_url)
        .map_err(|_| anyhow!("Invalid URL: {}", default_registry_url))?;

    match parsed_registry_url.scheme() {
        "file" => {
            local::delete_package(&default_registry_url, pkg_type, name, version, hash, out).await
        }
        "https" => {
            remote::delete_package(
                tman_config,
                &default_registry_url,
                pkg_type,
                name,
                version,
                hash,
                out,
            )
            .await
        }
        _ => Err(anyhow!("Unsupported URL scheme: {}", parsed_registry_url.scheme())),
    }
}
