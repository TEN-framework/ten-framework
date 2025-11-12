//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fs::{self, File},
    io::Read,
    path::{Path, PathBuf},
    str::FromStr,
    sync::Arc,
};

use anyhow::{anyhow, Context, Result};
use console::Emoji;
use semver::{Version, VersionReq};
use sha2::{Digest, Sha256};
use tempfile::NamedTempFile;
use ten_rust::pkg_info::{manifest::Manifest, pkg_type::PkgType, PkgInfo};
use walkdir::WalkDir;

use super::{
    found_result::{get_pkg_registry_info_from_manifest, PkgRegistryInfo},
    pkg_cache::{find_in_package_cache, store_file_to_package_cache},
};
use crate::{
    constants::{DEFAULT_REGISTRY_PAGE_SIZE, TEN_PACKAGE_FILE_EXTENSION},
    home::config::{is_verbose, TmanConfig},
    output::TmanOutput,
    registry::search::{matches_filter, PkgSearchFilter},
};

pub async fn upload_package(
    base_url: &str,
    package_file_path: &str,
    pkg_info: &PkgInfo,
    _out: Arc<Box<dyn TmanOutput>>,
) -> Result<String> {
    let mut path_url = url::Url::parse(base_url)
        .map_err(|e| anyhow!("Invalid file URL: {}", e))?
        .to_file_path()
        .map_err(|_| anyhow!("Failed to convert file URL to path"))?
        .to_string_lossy()
        .into_owned();

    // Ensure the base URL ends with a '/'.
    path_url = if path_url.ends_with('/') { path_url.to_string() } else { format!("{path_url}/") };

    // Construct the directory path.
    let dir_path = PathBuf::from(format!(
        "{}{}/{}/{}/",
        path_url,
        pkg_info.manifest.type_and_name.pkg_type,
        pkg_info.manifest.type_and_name.name,
        pkg_info.manifest.version.clone()
    ));

    // Check if the directory exists, and only create it if it doesn't.
    if !dir_path.exists() {
        fs::create_dir_all(&dir_path)
            .with_context(|| format!("Failed to create directory '{}'", dir_path.display()))?;
    }

    // Construct the full file path for the new location.
    let file_stem = Path::new(package_file_path)
        .file_stem()
        .ok_or_else(|| anyhow::anyhow!("Invalid file path provided"))?
        .to_str()
        .ok_or_else(|| anyhow::anyhow!("Invalid unicode in file path"))?;

    let extension = Path::new(package_file_path)
        .extension()
        .and_then(|ext| ext.to_str())
        .map(|ext| format!(".{ext}"))
        .unwrap_or_default();

    let new_file_name = format!("{}_{}{}", file_stem, pkg_info.hash, extension);

    let full_path = dir_path.join(new_file_name);

    // Copy the file to the new path.
    fs::copy(package_file_path, &full_path).with_context(|| {
        format!("Failed to copy file from '{}' to '{}'", package_file_path, full_path.display())
    })?;

    // Serialize and write the manifest to a JSON file.
    let manifest_json = pkg_info
        .serialize_manifest_with_resolved_content()
        .await
        .with_context(|| "Failed to serialize manifest to JSON")?;

    let manifest_file_name = format!("{}_{}_manifest.json", file_stem, pkg_info.hash);
    let manifest_path = dir_path.join(manifest_file_name);

    fs::write(&manifest_path, manifest_json)
        .with_context(|| format!("Failed to write manifest to '{}'", manifest_path.display()))?;

    Ok(full_path.to_string_lossy().to_string())
}

/// Calculate the hash of the file content to determine whether the file content
/// is the same when using the local registry.
fn calc_file_hash(path: &Path) -> Result<String> {
    let mut file = File::open(path)?;
    let mut hasher = Sha256::new();
    let mut buffer = [0u8; 8192];

    loop {
        let n = file.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        hasher.update(&buffer[..n]);
    }

    let hash = hasher.finalize();
    Ok(format!("{hash:x}"))
}

/// Check if a file exists and is a regular file.
pub fn check_file_exists(path: &Path) -> Result<()> {
    if !path.exists() {
        return Err(anyhow::anyhow!("File does not exist: {}", path.display()));
    }
    if !path.is_file() {
        return Err(anyhow::anyhow!("Path is not a file: {}", path.display()));
    }
    Ok(())
}

/// Extract the filename from a path.
pub fn extract_filename_from_path(path: &Path) -> Option<String> {
    path.file_name().map(|f| f.to_string_lossy().to_string())
}

/// Determine whether the locally cached file and the target file in the local
/// registry have the same hash.
fn is_same_file_by_hash(cache_file: &Path, registry_file_url: &str) -> Result<bool> {
    let registry_file_path = url::Url::parse(registry_file_url)
        .map_err(|e| anyhow::anyhow!("Invalid file URL: {}", e))?
        .to_file_path()
        .map_err(|_| anyhow::anyhow!("Failed to convert file URL to path"))?;

    if !registry_file_path.exists() {
        panic!(
            "Should not happen. The file does not exist in the local registry: {}",
            registry_file_path.display()
        );
    }

    let hash_cache = calc_file_hash(cache_file)?;
    let hash_registry = calc_file_hash(&registry_file_path)?;

    Ok(hash_cache == hash_registry)
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
    // First, try to retrieve the same package file from the cache.
    let registry_file_path = url::Url::parse(url)
        .map_err(|e| anyhow::anyhow!("Invalid file URL: {}", e))?
        .to_file_path()
        .map_err(|_| anyhow::anyhow!("Failed to convert file URL to path"))?;

    let file_name = registry_file_path
        .file_name()
        .ok_or_else(|| anyhow::anyhow!("downloaded file has invalid name"))?;

    if let Some(cached_file_path) =
        find_in_package_cache(pkg_type, pkg_name, pkg_version, &file_name.to_string_lossy())?
    {
        // We need to check whether the cached file and the target file have the
        // same content (i.e., the same hash).
        if let Ok(true) = is_same_file_by_hash(&cached_file_path, url) {
            // If the content is the same, directly copy the cached file to
            // `temp_path`.
            if is_verbose(tman_config.clone()).await {
                out.normal_line(&format!(
                    "{}  Found the package file ({}) in the package cache, using it directly.",
                    Emoji("🚀", ":-)"),
                    cached_file_path.to_string_lossy()
                ));
            }

            fs::copy(&cached_file_path, temp_path.path()).with_context(|| {
                format!("Failed to copy from cache {}", cached_file_path.display())
            })?;
            return Ok(());
        }
    }

    // Not found in the package cache, so proceed with the standard process to
    // retrieve it from the registry.

    let path_url_str = url::Url::parse(url)
        .map_err(|e| anyhow!("Invalid file URL: {}", e))?
        .to_file_path()
        .map_err(|_| anyhow!("Failed to convert file URL to path"))?
        .to_string_lossy()
        .into_owned();

    let path_url = PathBuf::from_str(&path_url_str)?;

    if !path_url.exists() {
        return Err(anyhow!("The package directory does not exist: {}", path_url.display()));
    }

    fs::copy(&path_url, temp_path.path())?;

    if tman_config.read().await.enable_package_cache {
        // Place the downloaded file into the cache.
        store_file_to_package_cache(
            pkg_type,
            pkg_name,
            pkg_version,
            &file_name.to_string_lossy(),
            &path_url,
        )?;
    }

    Ok(())
}

async fn find_file_with_criteria(
    base_url: &Path,
    pkg_type: Option<PkgType>,
    name: Option<&String>,
    version_req: Option<&VersionReq>,
    tags: Option<&Vec<String>>,
) -> Result<Vec<PkgRegistryInfo>> {
    let mut results = Vec::<PkgRegistryInfo>::new();

    // Determine which path to search based on pkg_type and name, and handle
    // search logic directly.
    match (pkg_type, name) {
        (Some(pkg_type), Some(name_str)) => {
            // Search specific pkg_type and name.
            let search_path = base_url.join(pkg_type.to_string());
            if search_path.exists() {
                let mut path_results =
                    search_versions(&search_path, name_str, version_req, tags).await?;
                results.append(&mut path_results);
            }
        }
        (Some(pkg_type), None) => {
            // Search all names under specific pkg_type.
            let search_path = base_url.join(pkg_type.to_string());
            if search_path.exists() {
                for entry in (std::fs::read_dir(&search_path)?).flatten() {
                    if entry.file_type()?.is_dir() {
                        let name_str = entry.file_name().to_string_lossy().to_string();
                        let mut name_results =
                            search_versions(&search_path, &name_str, version_req, tags).await?;
                        results.append(&mut name_results);
                    }
                }
            }
        }
        (None, Some(name)) => {
            // Search all package types for this name.
            for entry in (std::fs::read_dir(base_url)?).flatten() {
                if entry.file_type()?.is_dir() {
                    let type_dir = entry.path();
                    let name_dir = type_dir.join(name);
                    if name_dir.exists() {
                        let mut type_results =
                            search_versions(&type_dir, name, version_req, tags).await?;
                        results.append(&mut type_results);
                    }
                }
            }
        }
        (None, None) => {
            // Search all package types and names.

            for type_entry in (std::fs::read_dir(base_url)?).flatten() {
                if type_entry.file_type()?.is_dir() {
                    let type_dir = type_entry.path();
                    for name_entry in (std::fs::read_dir(&type_dir)?).flatten() {
                        if name_entry.file_type()?.is_dir() {
                            let mut name_results = search_versions(
                                &type_dir,
                                name_entry.file_name().to_string_lossy().as_ref(),
                                version_req,
                                tags,
                            )
                            .await?;
                            results.append(&mut name_results);
                        }
                    }
                }
            }
        }
    }

    Ok(results)
}

// Helper function to search for versions.
async fn search_versions(
    base_dir: &Path,
    name: &str,
    version_req: Option<&VersionReq>,
    tags: Option<&Vec<String>>,
) -> Result<Vec<PkgRegistryInfo>> {
    let mut results = Vec::<PkgRegistryInfo>::new();
    let target_path = base_dir.join(name);

    // Traverse the folders of all versions within the specified package.
    for version_dir in
        WalkDir::new(target_path).min_depth(1).max_depth(1).into_iter().filter_map(|e| e.ok())
    {
        let version_str = version_dir.file_name().to_str().unwrap_or_default();
        let version = match Version::parse(version_str) {
            Ok(v) => v,
            Err(_) => continue, // Skip invalid version directories.
        };

        // Check if the folder meets the version requirements.
        if version_req.is_none() || version_req.as_ref().is_some_and(|req| req.matches(&version)) {
            // Traverse the files within the folder of that version.
            for file in WalkDir::new(version_dir.path())
                .min_depth(1)
                .max_depth(1)
                .into_iter()
                .filter_map(|e| e.ok())
            {
                let path = file.path();

                // Look for .tpkg files.
                if path
                    .file_name()
                    .and_then(|f| f.to_str())
                    .is_some_and(|f| f.ends_with(TEN_PACKAGE_FILE_EXTENSION))
                {
                    // Get the file stem to look for the corresponding manifest
                    // file.
                    if let Some(file_stem) = path.file_stem().and_then(|f| f.to_str()) {
                        // Look for the matching manifest file:
                        // {file_stem}_manifest.json
                        let manifest_file_name = format!("{file_stem}_manifest.json");
                        let manifest_path = path.with_file_name(&manifest_file_name);

                        // Read the manifest file if it exists.
                        if manifest_path.exists() {
                            let manifest_content = fs::read_to_string(&manifest_path)
                                .with_context(|| {
                                    format!(
                                        "Failed to read manifest file: {}",
                                        manifest_path.display()
                                    )
                                })?;

                            let manifest = Manifest::create_from_str(&manifest_content)?;

                            // Check if the manifest meets the tags
                            // requirements.
                            if let Some(tag_filters) = tags {
                                if !tag_filters.is_empty() {
                                    // If manifest has no tags, skip this
                                    // package.
                                    if manifest.tags.is_none() {
                                        continue;
                                    }

                                    // If manifest has tags, check if it
                                    // contains all the required tags.
                                    let manifest_tags = manifest.tags.as_ref().unwrap();
                                    let all_tags_contained =
                                        tag_filters.iter().all(|tag| manifest_tags.contains(tag));

                                    if !all_tags_contained {
                                        continue;
                                    }
                                }
                            }

                            // Generate the download URL from the file path.
                            let download_url = url::Url::from_file_path(path)
                                .map_err(|_| anyhow!("Failed to convert path to file URL"))?
                                .to_string();

                            // Convert manifest to PkgRegistryInfo.
                            let mut pkg_registry_info: PkgRegistryInfo =
                                get_pkg_registry_info_from_manifest(&download_url, &manifest)
                                    .await?;

                            pkg_registry_info.download_url = download_url;

                            results.push(pkg_registry_info);
                        }
                    }
                }
            }
        }
    }

    Ok(results)
}

#[allow(clippy::too_many_arguments)]
pub async fn get_package_list(
    _tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    pkg_type: Option<PkgType>,
    name: Option<String>,
    version_req: Option<VersionReq>,
    tags: Option<Vec<String>>,
    _scope: Option<Vec<String>>,
    page_size: Option<u32>,
    page: Option<u32>,
    _out: &Arc<Box<dyn TmanOutput>>,
) -> Result<Vec<PkgRegistryInfo>> {
    let mut path_url = url::Url::parse(base_url)
        .map_err(|e| anyhow!("Invalid file URL: {}", e))?
        .to_file_path()
        .map_err(|_| anyhow!("Failed to convert file URL to path"))?
        .to_string_lossy()
        .into_owned();

    // Ensure the base URL ends with a '/'.
    path_url = if path_url.ends_with('/') { path_url.to_string() } else { format!("{path_url}/") };

    let version_req_ref = version_req.as_ref();
    let name_ref = name.as_ref();

    // Get all matching packages
    let all_results = find_file_with_criteria(
        Path::new(&path_url),
        pkg_type,
        name_ref,
        version_req_ref,
        tags.as_ref(),
    )
    .await?;

    // If page is specified, paginate the results.
    if let Some(page_num) = page {
        let page_size_value = page_size.unwrap_or(DEFAULT_REGISTRY_PAGE_SIZE) as usize;
        let start_idx = (page_num as usize - 1) * page_size_value;

        // Return empty result if start index is beyond the array length.
        if start_idx >= all_results.len() {
            return Ok(Vec::new());
        }

        let end_idx = std::cmp::min(start_idx + page_size_value, all_results.len());
        Ok(all_results[start_idx..end_idx].to_vec())
    } else {
        // If no page is specified, return all results.
        Ok(all_results)
    }
}

#[allow(clippy::too_many_arguments)]
pub async fn search_packages(
    _tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    filter: &PkgSearchFilter,
    page_size: Option<u32>,
    page: Option<u32>,
    sort_by: Option<&str>,
    sort_order: Option<&str>,
    _scope: Option<&str>,
    _out: &Arc<Box<dyn TmanOutput>>,
) -> Result<(u32, Vec<PkgRegistryInfo>)> {
    let mut path_url = url::Url::parse(base_url)
        .map_err(|e| anyhow!("Invalid file URL: {}", e))?
        .to_file_path()
        .map_err(|_| anyhow!("Failed to convert file URL to path"))?
        .to_string_lossy()
        .into_owned();
    path_url = if path_url.ends_with('/') { path_url } else { format!("{path_url}/") };
    let base_path = Path::new(&path_url);
    let all_packages = find_file_with_criteria(base_path, None, None, None, None).await?;
    let mut filtered =
        all_packages.into_iter().filter(|p| matches_filter(p, &filter.filter)).collect::<Vec<_>>();
    // Sort
    if sort_by.is_some_and(|s| s == "name") {
        filtered.sort_by(|a, b| {
            let cmp = a.basic_info.type_and_name.name.cmp(&b.basic_info.type_and_name.name);
            if sort_order.is_some_and(|s| s == "asc") {
                cmp
            } else {
                cmp.reverse()
            }
        });
    } // add more sort fields if needed

    // For local registry, we always return all fields of a package no matter
    // what the scope is.

    let total = filtered.len() as u32;
    // Paginate
    let page_size = page_size.unwrap_or(10);
    let page = page.unwrap_or(1);
    let start = ((page - 1) * page_size) as usize;
    if start >= filtered.len() {
        return Ok((total, vec![]));
    }
    let end = std::cmp::min(start + page_size as usize, filtered.len());
    let results = filtered[start..end].to_vec();
    Ok((total, results))
}

pub async fn delete_package(
    base_url: &str,
    pkg_type: PkgType,
    name: &String,
    version: &Version,
    hash: &String,
    _out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    let mut path_url = url::Url::parse(base_url)
        .map_err(|e| anyhow!("Invalid file URL: {}", e))?
        .to_file_path()
        .map_err(|_| anyhow!("Failed to convert file URL to path"))?
        .to_string_lossy()
        .into_owned();

    // Ensure the base URL ends with a '/'.
    path_url = if path_url.ends_with('/') { path_url.to_string() } else { format!("{path_url}/") };

    // Construct the directory path.
    let dir_path = PathBuf::from(format!("{path_url}{pkg_type}/{name}/{version}/"));

    if dir_path.exists() {
        // Iterate over the files in the directory.
        for entry in fs::read_dir(&dir_path)
            .with_context(|| format!("Failed to read directory '{}'", dir_path.display()))?
        {
            let entry = entry.with_context(|| {
                format!("Failed to read entry in directory '{}'", dir_path.display())
            })?;
            let file_path = entry.path();

            // Check if the file name matches the pattern.
            if let Some(file_name) = file_path.file_name().and_then(|name| name.to_str()) {
                if let Some(file_stem) =
                    Path::new(file_name).file_stem().and_then(|stem| stem.to_str())
                {
                    if file_stem.ends_with(&format!("_{hash}")) {
                        // Delete the file.
                        fs::remove_file(&file_path).with_context(|| {
                            format!("Failed to delete file '{}'", file_path.display())
                        })?;
                    }
                }
            }
        }
    }

    Ok(())
}
