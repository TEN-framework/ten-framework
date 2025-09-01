//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, fs, io::Write, sync::Arc, time::Duration};

use anyhow::{anyhow, Context, Result};
use console::Emoji;
use reqwest::header::{HeaderMap, AUTHORIZATION, CONTENT_TYPE};
use semver::{Version, VersionReq};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use tempfile::NamedTempFile;
use ten_rust::pkg_info::{pkg_basic_info::PkgBasicInfo, pkg_type::PkgType, PkgInfo};
use tokio::{sync::RwLock, time::sleep};

use super::pkg_cache::{find_in_package_cache, store_file_to_package_cache};
use crate::{
    constants::{
        ADMIN_TOKEN, DEFAULT_REGISTRY_PAGE_SIZE, REMOTE_REGISTRY_MAX_RETRIES,
        REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS, REMOTE_REGISTRY_RETRY_DELAY_MS,
    },
    home::config::{is_verbose, TmanConfig},
    http::create_http_client_with_proxies,
    output::TmanOutput,
    registry::{found_result::PkgRegistryInfo, search::PkgSearchFilter},
};

/// Checks if a package requires admin token authorization based on its tags.
///
/// Returns true if the package has any tag starting with "ten:".
fn requires_admin_token_based_on_tags(pkg_info: &PkgInfo) -> bool {
    pkg_info
        .manifest
        .tags
        .as_ref()
        .map(|tags| tags.iter().any(|tag| tag.starts_with("ten:")))
        .unwrap_or(false)
}

async fn retry_async<'a, F, T>(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    max_retries: u32,
    retry_delay: Duration,
    out: Arc<Box<dyn TmanOutput>>,
    mut operation: F,
) -> Result<T>
where
    F: FnMut() -> std::pin::Pin<Box<dyn std::future::Future<Output = Result<T>> + Send + 'a>>,
{
    for attempt in 0..=max_retries {
        match operation().await {
            Ok(result) => return Ok(result),
            Err(e) => {
                if is_verbose(tman_config.clone()).await {
                    out.normal_line(&format!("Attempt {} failed: {:?}", attempt + 1, e));
                }

                if attempt == max_retries {
                    return Err(e);
                }

                sleep(retry_delay).await;
            }
        }
    }

    unreachable!("retry logic should either return or error out")
}

#[derive(Debug)]
struct UploadInfo {
    resource_id: String,
    url: String,
}

async fn get_package_upload_info(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    client: &reqwest::Client,
    pkg_info: &PkgInfo,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<UploadInfo> {
    // Basically, the principle here is that when tman install is run, all the
    // necessary metadata for a package (such as the package's dependencies,
    // support, etc.) should be uploaded to the registry during tman publish and
    // stored by the registry. This way, when tman install is run and the
    // package's metadata is requested, the registry can return it directly,
    // without tman needing to request the actual zip file from the registry,
    // extract the manifest.json from it, and then retrieve the information.
    // That would be too cumbersome. Therefore, information not needed during
    // tman install, such as api/schema, does not need to be uploaded to the
    // registry during tman publish. Of course, if it is uploaded, the potential
    // benefit is that this information can be directly displayed in the cloud
    // store.

    // TODO: Implement the 'environments' mechanism.
    // Add a new environments field, or perhaps directly upload the content of
    // manifest.json to the cloud store.

    let max_retries = REMOTE_REGISTRY_MAX_RETRIES;
    let retry_delay = Duration::from_millis(REMOTE_REGISTRY_RETRY_DELAY_MS);

    retry_async(tman_config.clone(), max_retries, retry_delay, out.clone(), || {
        let base_url = base_url.to_string();
        let client = client.clone();
        let pkg_info = pkg_info.clone();
        let out = out.clone();
        let tman_config = tman_config.clone();

        Box::pin(async move {
            // Resolve LocaleContent fields before creating payload
            let mut description = None;
            if let Some(desc) = &pkg_info.manifest.description {
                let mut resolved_locales = HashMap::new();
                for (locale, locale_content) in &desc.locales {
                    let content = locale_content
                        .get_content()
                        .await
                        .map_err(|e| anyhow!("Failed to resolve description content: {}", e))?;
                    let resolved_content = ten_rust::pkg_info::manifest::LocaleContent {
                        content: Some(content),
                        import_uri: None,
                        base_dir: None,
                    };
                    resolved_locales.insert(locale.clone(), resolved_content);
                }
                description = Some(ten_rust::pkg_info::manifest::LocalizedField {
                    locales: resolved_locales,
                });
            }

            let mut display_name = None;
            if let Some(name) = &pkg_info.manifest.display_name {
                let mut resolved_locales = HashMap::new();
                for (locale, locale_content) in &name.locales {
                    let content = locale_content
                        .get_content()
                        .await
                        .map_err(|e| anyhow!("Failed to resolve display_name content: {}", e))?;
                    let resolved_content = ten_rust::pkg_info::manifest::LocaleContent {
                        content: Some(content),
                        import_uri: None,
                        base_dir: None,
                    };
                    resolved_locales.insert(locale.clone(), resolved_content);
                }
                display_name = Some(ten_rust::pkg_info::manifest::LocalizedField {
                    locales: resolved_locales,
                });
            }

            let mut readme = None;
            if let Some(rm) = &pkg_info.manifest.readme {
                let mut resolved_locales = HashMap::new();
                for (locale, locale_content) in &rm.locales {
                    let content = locale_content
                        .get_content()
                        .await
                        .map_err(|e| anyhow!("Failed to resolve readme content: {}", e))?;
                    let resolved_content = ten_rust::pkg_info::manifest::LocaleContent {
                        content: Some(content),
                        import_uri: None,
                        base_dir: None,
                    };
                    resolved_locales.insert(locale.clone(), resolved_content);
                }
                readme = Some(ten_rust::pkg_info::manifest::LocalizedField {
                    locales: resolved_locales,
                });
            }

            let payload = json!(PkgRegistryInfo {
                basic_info: PkgBasicInfo::from(&pkg_info),
                dependencies: match &pkg_info.manifest.dependencies {
                    Some(deps) => deps.clone(),
                    None => vec![],
                },
                hash: pkg_info.hash.clone(),
                download_url: String::new(),
                content_format: Some("gzip".to_string()),
                tags: pkg_info.manifest.tags.clone(),
                description,
                display_name,
                readme,
            });

            if is_verbose(tman_config.clone()).await {
                out.normal_line(&format!("Payload of publishing: {payload}"));
            }

            let mut headers = HeaderMap::new();

            // Check if the package requires admin token based on its tags.
            let requires_admin = requires_admin_token_based_on_tags(&pkg_info);

            if requires_admin {
                // If a tag starts with "ten:", we must use admin_token.
                if let Some(admin_token) = &tman_config.read().await.admin_token {
                    let basic_token = format!("Basic {admin_token}");
                    headers.insert(
                        ADMIN_TOKEN,
                        basic_token.parse().map_err(|e| {
                            out.error_line(&format!("Failed to parse authorization token: {e}"));
                            e
                        })?,
                    );
                } else {
                    if is_verbose(tman_config.clone()).await {
                        out.normal_line(
                            "Admin token is required for packages with 'ten:' tags but is missing",
                        );
                    }
                    return Err(anyhow!(
                        "Admin token is required for packages with 'ten:' tags but is missing"
                    ));
                }
            }

            if let Some(user_token) = &tman_config.read().await.user_token {
                let basic_token = format!("Basic {user_token}");
                headers.insert(
                    AUTHORIZATION,
                    basic_token.parse().map_err(|e| {
                        out.error_line(&format!("Failed to parse authorization token: {e}"));
                        e
                    })?,
                );
            } else {
                if is_verbose(tman_config.clone()).await {
                    out.normal_line("Authorization token is missing");
                }
                return Err(anyhow!("Authorization token is missing"));
            }

            let response = client
                .post(base_url)
                .headers(headers)
                .timeout(Duration::from_secs(REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS))
                .json(&payload)
                .send()
                .await
                .map_err(|e| anyhow!("Failed to send request: {}", e))?;

            if !response.status().is_success() {
                return Err(anyhow!("Received non-success status code: {}", response.status(),));
            }

            let data: Value = response
                .json()
                .await
                .map_err(|e| anyhow!("Failed to parse JSON response: {}", e))?;

            let resource_id = data["data"]["resourceId"]
                .as_str()
                .ok_or_else(|| anyhow!("Missing 'resourceId' in response"))?
                .to_string();
            let url = data["data"]["url"]
                .as_str()
                .ok_or_else(|| anyhow!("Missing 'url' in response"))?
                .to_string();

            Ok(UploadInfo {
                resource_id,
                url,
            })
        })
    })
    .await
}

async fn upload_package_to_remote(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    client: &reqwest::Client,
    package_file_path: &str,
    url: &str,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    let max_retries = REMOTE_REGISTRY_MAX_RETRIES;
    let retry_delay = Duration::from_millis(REMOTE_REGISTRY_RETRY_DELAY_MS);

    retry_async(tman_config, max_retries, retry_delay, out.clone(), || {
        let client = client.clone();
        let package_file_path = package_file_path.to_string();
        let url = url.to_string();
        let out = out.clone();

        Box::pin(async move {
            let body = match std::fs::read(&package_file_path) {
                Ok(file) => file,
                Err(e) => {
                    out.error_line(&format!(
                        "Failed to read file at '{}': {}",
                        &package_file_path, e
                    ));
                    return Err(e.into());
                }
            };

            let mut headers = HeaderMap::new();

            headers.insert(
                CONTENT_TYPE,
                "application/gzip".parse().map_err(|e| {
                    out.error_line(&format!("Failed to parse content type: {e}"));
                    e
                })?,
            );

            let response = client.put(url).headers(headers).body(body).send().await;
            match response {
                Ok(resp) => {
                    if resp.status().is_success() {
                        Ok(())
                    } else {
                        Err(anyhow!(
                            "Failed to upload package with HTTP status {:?}",
                            resp.text().await
                        ))
                    }
                }
                Err(e) => Err(e.into()),
            }
        })
    })
    .await
}

async fn ack_of_uploading(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    client: &reqwest::Client,
    resource_id: &str,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    let max_retries = REMOTE_REGISTRY_MAX_RETRIES;
    let retry_delay = Duration::from_millis(REMOTE_REGISTRY_RETRY_DELAY_MS);

    retry_async(tman_config, max_retries, retry_delay, out.clone(), || {
        let client = client.clone();
        let base_url = base_url.to_string();
        let resource_id = resource_id.to_string();
        let out = out.clone();

        Box::pin(async move {
            let url = match reqwest::Url::parse(&base_url) {
                Ok(url) => url,
                Err(e) => {
                    out.error_line(&format!("Failed to parse URL: {e}"));
                    return Err(e.into());
                }
            };

            let body = json!({ "resourceId": resource_id });

            let response = client
                .patch(url)
                .timeout(Duration::from_secs(REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS))
                .json(&body)
                .send()
                .await;

            match response {
                Ok(resp) => {
                    if resp.status().is_success() {
                        Ok(())
                    } else {
                        Err(anyhow!(
                            "Failed to acknowledge uploading with HTTP status {}",
                            resp.status()
                        ))
                    }
                }
                Err(e) => Err(e.into()),
            }
        })
    })
    .await
}

pub async fn upload_package(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    package_file_path: &str,
    pkg_info: &PkgInfo,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<String> {
    let client = create_http_client_with_proxies()?;

    let upload_info =
        get_package_upload_info(tman_config.clone(), base_url, &client, pkg_info, out.clone())
            .await?;

    upload_package_to_remote(
        tman_config.clone(),
        &client,
        package_file_path,
        &upload_info.url,
        out.clone(),
    )
    .await?;

    ack_of_uploading(tman_config.clone(), base_url, &client, &upload_info.resource_id, out.clone())
        .await?;

    Ok(upload_info.url)
}

// Parse the header of content-range.
fn parse_content_range(content_range: &str) -> Option<(u64, u64, u64)> {
    let parts: Vec<&str> = content_range.split('/').collect();
    if parts.len() == 2 {
        let total_size = parts[1].parse().ok()?;
        let range_parts: Vec<&str> = parts[0].split('-').collect();
        if range_parts.len() == 2 {
            let start = range_parts[0].parse().ok()?;
            let end = range_parts[1].parse().ok()?;
            return Some((start, end, total_size));
        }
    }
    None
}

pub async fn get_package(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pkg_type: &PkgType,
    pkg_name: &str,
    pkg_version: &Version,
    url: &str,
    temp_file: &mut NamedTempFile,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    // First, check the cache. If there is a matching filename, use the cached
    // file directly.
    let parsed_url = url::Url::parse(url).map_err(|e| anyhow::anyhow!("Invalid URL: {}", e))?;
    let file_name = parsed_url
        .path_segments()
        .and_then(|mut segments| segments.next_back())
        .ok_or_else(|| anyhow::anyhow!("Failed to extract file name from URL"))?;

    if let Some(cached_file_path) =
        find_in_package_cache(pkg_type, pkg_name, pkg_version, file_name)?
    {
        // If the filename matches, directly copy the cached file to
        // `temp_file`.
        if is_verbose(tman_config.clone()).await {
            out.normal_line(&format!(
                "{}  Found the package file ({}) in the package cache, using it directly.",
                Emoji("🚀", ":-)"),
                cached_file_path.to_string_lossy()
            ));
        }

        fs::copy(&cached_file_path, temp_file.path())
            .with_context(|| format!("Failed to copy from cache {}", cached_file_path.display()))?;
        return Ok(());
    }

    // Not found in the package cache, so proceed with the standard process to
    // retrieve it from the registry.

    let client = create_http_client_with_proxies()?;

    let temp_file = Arc::new(RwLock::new(temp_file));

    let max_retries = REMOTE_REGISTRY_MAX_RETRIES;
    let retry_delay = Duration::from_millis(REMOTE_REGISTRY_RETRY_DELAY_MS);

    let download_complete = Arc::new(RwLock::new(false));

    retry_async(tman_config.clone(), max_retries, retry_delay, out.clone(), || {
        let client = client.clone();
        let url = url.to_string();
        let temp_file = Arc::clone(&temp_file);
        let download_complete = Arc::clone(&download_complete);

        Box::pin(async move {
            // Check the size of the file that has already been downloaded.
            let temp_file_len = {
                let temp_file_read = temp_file.read().await;
                temp_file_read.as_file().metadata().map(|metadata| metadata.len()).unwrap_or(0)
            };

            // Set the Range header to support resumable downloads.
            let mut headers = HeaderMap::new();
            if temp_file_len > 0 {
                headers.insert(
                    reqwest::header::RANGE,
                    format!("bytes={temp_file_len}-").parse().unwrap(),
                );
            }

            let response = client
                .get(&url)
                .headers(headers)
                .timeout(Duration::from_secs(REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS))
                .send()
                .await
                .with_context(|| format!("Failed to send GET request to {url}"))?;

            if !response.status().is_success()
                && response.status() != reqwest::StatusCode::PARTIAL_CONTENT
            {
                return Err(anyhow!(
                    "Failed to download the package from {}: HTTP {}.",
                    url,
                    response.status()
                ));
            }

            // Get the headers of Content-Range or Content-Length.
            let content_range = response.headers().get(reqwest::header::CONTENT_RANGE).cloned();
            let content_length = response.headers().get(reqwest::header::CONTENT_LENGTH).cloned();

            // Read the content of the response and append the newly
            // downloaded part to the file.
            let content =
                response.bytes().await.with_context(|| "Failed to read bytes from response")?;

            if content.is_empty() {
                return Err(anyhow!("No new content downloaded"));
            }

            let mut temp_file_write = temp_file.write().await;
            temp_file_write
                .as_file_mut()
                .write_all(&content)
                .with_context(|| "Failed to write content to temporary file")?;

            // Check if we have downloaded the entire file.
            if let Some(content_range) = content_range {
                // Parse the content-range to check if download is complete.
                let content_range_str = content_range.to_str().unwrap();
                if let Some((_, _, total_size)) = parse_content_range(content_range_str) {
                    if temp_file_len + content.len() as u64 >= total_size {
                        *download_complete.write().await = true;
                    }
                }
            } else if content_length.is_some() {
                // If there is no content-range but content-length exists,
                // the download should be complete in
                // one go.
                *download_complete.write().await = true;
            }

            Ok(())
        })
    })
    .await?;

    // Only print when `download_complete` is `true`.
    if *download_complete.read().await {
        let temp_file_borrow = temp_file.read().await;
        if is_verbose(tman_config.clone()).await {
            out.normal_line(&format!(
                "Package downloaded successfully from {} and written to {}",
                url,
                temp_file_borrow.path().display()
            ));
        }

        if tman_config.read().await.enable_package_cache {
            // Place the downloaded file into the cache.
            let downloaded_path = temp_file_borrow.path();
            store_file_to_package_cache(
                pkg_type,
                pkg_name,
                pkg_version,
                file_name,
                downloaded_path,
            )?;
        }
    }

    Ok(())
}

#[derive(Debug, Serialize, Deserialize)]
struct ApiResponse {
    status: String,
    data: RegistryPackagesData,
    meta: Value,
}

#[derive(Debug, Serialize, Deserialize)]
struct RegistryPackagesData {
    #[serde(rename = "totalSize")]
    total_size: u32,

    packages: Vec<PkgRegistryInfo>,
}

/// Retrieves a list of packages from a remote registry that match the specified
/// criteria.
///
/// This function queries the remote registry API with pagination to fetch all
/// packages that match the given package type, name, and version requirement.
///
/// # Arguments
/// * `tman_config` - Configuration containing registry settings and verbose
///   flag.
/// * `base_url` - Base URL of the remote registry.
/// * `pkg_type` - Optional type of package to search for (e.g., app,
///   extension).
/// * `name` - Optional name of the package to search for.
/// * `version_req` - Optional version requirement to filter packages.
/// * `tags` - Optional tags to filter packages.
/// * `page_size` - Optional page size for pagination.
/// * `page` - Optional page number for pagination.
/// * `out` - Output interface for logging.
///
/// # Returns
/// A vector of `PkgRegistryInfo` containing information about matching
/// packages.
///
/// # Errors
/// * If the HTTP client creation fails.
/// * If the request to the registry fails.
/// * If the API response has a non-OK status.
/// * If parsing the JSON response fails.
#[allow(clippy::too_many_arguments)]
pub async fn get_package_list(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    pkg_type: Option<PkgType>,
    name: Option<String>,
    version_req: Option<VersionReq>,
    tags: Option<Vec<String>>,
    scope: Option<Vec<String>>,
    page_size: Option<u32>,
    page: Option<u32>,
    out: &Arc<Box<dyn TmanOutput>>,
) -> Result<Vec<PkgRegistryInfo>> {
    let max_retries = REMOTE_REGISTRY_MAX_RETRIES;
    let retry_delay = Duration::from_millis(REMOTE_REGISTRY_RETRY_DELAY_MS);

    retry_async(tman_config.clone(), max_retries, retry_delay, out.clone(), || {
        let client = match create_http_client_with_proxies() {
            Ok(c) => c,
            Err(e) => return Box::pin(async { Err(e) }),
        };

        let out = out.clone();
        let version_req = version_req.clone();
        let name = name.clone();
        let tags = tags.clone();
        let scope = scope.clone();
        let tman_config = tman_config.clone();

        Box::pin(async move {
            let mut results = Vec::new();
            // If page is specified, we'll fetch only that page.
            // Otherwise, we'll start from page 1 and fetch all pages.
            let mut current_page = page.unwrap_or(1);
            let mut total_size;
            // Determine if we should fetch multiple pages or just one.
            let fetch_single_page = page.is_some();
            // Use provided page_size or default to
            // DEFAULT_REGISTRY_PAGE_SIZE.
            let page_size_value = page_size.unwrap_or(DEFAULT_REGISTRY_PAGE_SIZE);

            loop {
                // Build the URL with query parameters for pagination and
                // filtering.
                let mut url = reqwest::Url::parse(base_url)?;
                {
                    let mut query = url.query_pairs_mut();

                    // Only add parameters if they are provided
                    if let Some(pt) = &pkg_type {
                        query.append_pair("type", &pt.to_string());
                    }

                    if let Some(n) = &name {
                        query.append_pair("name", n);
                    }

                    if let Some(vr) = &version_req {
                        query.append_pair("version", &vr.to_string());
                    }

                    // Add tags parameter if provided.
                    if let Some(t) = &tags {
                        if !t.is_empty() {
                            query.append_pair("tags", &t.join(","));
                        }
                    }

                    // Add scope parameter if provided.
                    if let Some(s) = &scope {
                        if !s.is_empty() {
                            query.append_pair("scope", &s.join(","));
                        }
                    }

                    // Pagination parameters.
                    query
                        .append_pair("pageSize", &page_size_value.to_string())
                        .append_pair("page", &current_page.to_string());
                } // query is dropped here

                if is_verbose(tman_config.clone()).await {
                    let query_info = format!(
                        "{}{}{}{}{}",
                        pkg_type.as_ref().map_or("".to_string(), |pt| format!("type={pt} ")),
                        name.as_ref().map_or("".to_string(), |n| format!("name={n} ")),
                        version_req.as_ref().map_or("".to_string(), |vr| format!("version={vr} ")),
                        tags.as_ref().map_or("".to_string(), |t| {
                            if t.is_empty() {
                                "".to_string()
                            } else {
                                format!("tags={}", t.join(","))
                            }
                        }),
                        scope.as_ref().map_or("".to_string(), |s| {
                            if s.is_empty() {
                                "".to_string()
                            } else {
                                format!("scope={}", s.join(","))
                            }
                        })
                    );

                    out.normal_line(&format!(
                        "Fetching page {} with page size {} and query params: {}",
                        current_page,
                        page_size_value,
                        query_info.trim()
                    ));
                }

                // Send the request with timeout.
                let response = client
                    .get(url)
                    .timeout(Duration::from_secs(REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS))
                    .send()
                    .await;

                let response = match response {
                    Ok(response) => response,
                    Err(e) => {
                        return Err(anyhow::anyhow!("Request failed: {}", e));
                    }
                };

                if response.status() != reqwest::StatusCode::OK {
                    return Err(anyhow::anyhow!(
                        "API responded with non-ok status: {}",
                        response.status()
                    ));
                }

                // Parse the response
                let body = response.text().await?;
                let api_response = serde_json::from_str::<ApiResponse>(&body);
                let api_response = match api_response {
                    Ok(api_response) => api_response,
                    Err(e) => {
                        return Err(anyhow::anyhow!("Failed to parse JSON response: {}", e));
                    }
                };

                if api_response.status != "ok" {
                    return Err(anyhow::anyhow!(
                        "API responded with non-ok status: {}",
                        api_response.status
                    ));
                }

                // Update total size and collect packages.
                total_size = api_response.data.total_size as usize;
                let packages_is_empty = api_response.data.packages.is_empty();
                results.extend(api_response.data.packages);

                if is_verbose(tman_config.clone()).await {
                    out.normal_line(&format!(
                        "Fetched {} packages (total: {}) at page {} for {}:{}@{}",
                        results.len(),
                        total_size,
                        current_page,
                        pkg_type.as_ref().map_or("".to_string(), |pt| pt.to_string()),
                        name.as_ref().map_or("".to_string(), |n| n.clone()),
                        version_req.as_ref().map_or("".to_string(), |vr| vr.to_string())
                    ));
                }

                // If we're only fetching a single page or we've reached the
                // end, break the loop.
                if fetch_single_page || results.len() >= total_size || packages_is_empty {
                    break;
                }

                current_page += 1;
            }

            Ok(results)
        })
    })
    .await
}

#[allow(clippy::too_many_arguments)]
pub async fn search_packages(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    filter: &PkgSearchFilter,
    page_size: Option<u32>,
    page: Option<u32>,
    sort_by: Option<&str>,
    sort_order: Option<&str>,
    scope: Option<&str>,
    out: &Arc<Box<dyn TmanOutput>>,
) -> Result<(u32, Vec<PkgRegistryInfo>)> {
    let max_retries = REMOTE_REGISTRY_MAX_RETRIES;
    let retry_delay = Duration::from_millis(REMOTE_REGISTRY_RETRY_DELAY_MS);

    retry_async(tman_config.clone(), max_retries, retry_delay, out.clone(), || {
        let client = match create_http_client_with_proxies() {
            Ok(c) => c,
            Err(e) => return Box::pin(async { Err(e) }),
        };

        let out = out.clone();
        let filter = filter.clone();
        let sort_by = sort_by.map(|s| s.to_string());
        let sort_order = sort_order.map(|s| s.to_string());
        let tman_config = tman_config.clone();
        let url = format!("{}/search", base_url.trim_end_matches('/'));

        Box::pin(async move {
            let mut results = Vec::new();

            // If page is specified, we'll fetch only that page.
            // Otherwise, we'll start from page 1 and fetch all pages.
            let mut current_page = page.unwrap_or(1);
            let mut total_size: u32;
            // Determine if we should fetch multiple pages or just one.
            let fetch_single_page = page.is_some();
            // Use provided page_size or default to
            // DEFAULT_REGISTRY_PAGE_SIZE.
            let page_size_value = page_size.unwrap_or(DEFAULT_REGISTRY_PAGE_SIZE);

            loop {
                let body = json!({
                    "filter": filter,
                    "options": {
                        "pageSize": page_size_value,
                        "page": current_page,
                        "sortBy": sort_by,
                        "sortOrder": sort_order,
                        "scope": scope
                    }
                });

                let response = client
                    .post(&url)
                    .json(&body)
                    .timeout(Duration::from_secs(REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS))
                    .send()
                    .await?;

                if !response.status().is_success() {
                    return Err(anyhow!("Request failed with status: {}", response.status()));
                }

                // Parse the response
                let body = response.text().await?;
                let api_response = serde_json::from_str::<ApiResponse>(&body)?;

                if api_response.status != "ok" {
                    return Err(anyhow!("API error: {}", api_response.status));
                }

                if is_verbose(tman_config.clone()).await {
                    out.normal_line(&format!(
                        "Searched {} packages (total: {}) at page {}",
                        api_response.data.packages.len(),
                        api_response.data.total_size,
                        current_page,
                    ));
                }

                // Update total size and collect packages.
                total_size = api_response.data.total_size;
                let packages_is_empty = api_response.data.packages.is_empty();
                results.extend(api_response.data.packages);

                // If we're only fetching a single page or we've reached the
                // end, break the loop.
                if fetch_single_page || results.len() >= total_size as usize || packages_is_empty {
                    break;
                }

                current_page += 1;
            }

            Ok((total_size, results))
        })
    })
    .await
}

pub async fn delete_package(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    base_url: &str,
    pkg_type: PkgType,
    name: &str,
    version: &Version,
    hash: &str,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    let max_retries = REMOTE_REGISTRY_MAX_RETRIES;
    let retry_delay = Duration::from_millis(REMOTE_REGISTRY_RETRY_DELAY_MS);

    retry_async(tman_config.clone(), max_retries, retry_delay, out.clone(), || {
        let client = match create_http_client_with_proxies() {
            Ok(c) => c,
            Err(e) => return Box::pin(async { Err(e) }),
        };

        let base_url = base_url.to_string();
        let version = version.clone();
        let hash = hash.to_string();
        let out = out.clone();
        let tman_config = tman_config.clone();

        Box::pin(async move {
            // Ensure the base URL ends with a '/'.
            let base_url = if base_url.ends_with('/') { base_url } else { format!("{base_url}/") };

            let url = reqwest::Url::parse(&base_url).inspect_err(|&e| {
                out.error_line(&format!("Failed to parse base URL: {e}"));
            })?;

            let url = url
                .join(&format!("{}/{}/{}/{}", &pkg_type.to_string(), &name, version, hash,))
                .inspect_err(|&e| {
                    out.error_line(&format!("Failed to join URL path: {e}"));
                })?;

            let mut headers = HeaderMap::new();

            if let Some(admin_token) = &tman_config.read().await.admin_token {
                let basic_token = format!("Basic {admin_token}");
                headers.insert(
                    ADMIN_TOKEN,
                    basic_token.parse().map_err(|e| {
                        out.error_line(&format!("Failed to parse authorization token: {e}"));
                        e
                    })?,
                );
            } else {
                if is_verbose(tman_config.clone()).await {
                    out.normal_line("Authorization token is missing");
                }
                return Err(anyhow!("Authorization token is missing"));
            }

            // Sending the DELETE request.
            let response = client
                .delete(url)
                .headers(headers)
                .timeout(Duration::from_secs(REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS))
                .send()
                .await;

            match response {
                Ok(resp) => {
                    if resp.status().is_success() {
                        Ok(())
                    } else {
                        Err(anyhow!("HTTP error: {}", resp.status()))
                    }
                }
                Err(e) => Err(e.into()),
            }
        })
    })
    .await
}
