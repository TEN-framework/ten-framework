//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::{anyhow, Context, Result};
use url::Url;

use crate::utils::fs::read_file_to_string;

/// Loads content from a file:// URL.
fn load_content_from_file_url(url: &Url) -> Result<String> {
    // Convert file URL to local path
    let path = url.to_file_path().map_err(|_| anyhow!("Invalid file URL: {}", url))?;

    // Read the content file.
    read_file_to_string(&path)
        .with_context(|| format!("Failed to read content file from {}", path.display()))
}

/// Loads content from an HTTP/HTTPS URL.
async fn load_content_from_http_url(url: &url::Url) -> Result<String> {
    // Create HTTP client
    let client = reqwest::Client::new();

    // Make HTTP request
    let response = client
        .get(url.as_str())
        .send()
        .await
        .with_context(|| format!("Failed to send HTTP request to {url}"))?;

    // Check if request was successful
    if !response.status().is_success() {
        return Err(anyhow!("HTTP request failed with status {}: {}", response.status(), url));
    }

    // Get response body as text
    response.text().await.with_context(|| format!("Failed to read response body from {url}"))
}

/// Load content from a URI.
///
/// The URI can be a relative path or a URL.
pub async fn load_content_from_uri(uri: &str) -> Result<String> {
    // Try to parse as URL.
    if let Ok(url) = Url::parse(uri) {
        match url.scheme() {
            "http" | "https" => {
                return load_content_from_http_url(&url).await;
            }
            "file" => {
                return load_content_from_file_url(&url);
            }
            _ => {
                #[cfg(windows)]
                // Windows drive letter
                if url.scheme().len() == 1
                    && url.scheme().chars().next().unwrap().is_ascii_alphabetic()
                {
                    // The uri may be a relative path in Windows.
                    // Continue to parse the uri as a relative path.
                } else {
                    return Err(anyhow::anyhow!(
                        "Unsupported URL scheme '{}' in uri: {} when load_content_from_uri",
                        url.scheme(),
                        uri
                    ));
                }

                #[cfg(not(windows))]
                return Err(anyhow::anyhow!(
                    "Unsupported URL scheme '{}' in uri: {} when load_content_from_uri",
                    url.scheme(),
                    uri
                ));
            }
        }
    }

    // It's a file path, read the content file.
    let content = read_file_to_string(uri)
        .with_context(|| format!("Failed to read content file from {uri}"))?;

    Ok(content)
}
