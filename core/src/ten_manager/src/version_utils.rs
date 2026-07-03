//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::time::Duration;

use anyhow::{anyhow, Result};
use semver::{Version, VersionReq};
use serde_json::Value;

use crate::{
    constants::{GITHUB_RELEASE_REQUEST_TIMEOUT_SECS, GITHUB_RELEASE_URL},
    http::create_http_client_with_proxies,
    version::VERSION,
};

/// Used to parse a pattern like `aaa@3.0.0` and return `aaa` and `3.0.0`.
pub fn parse_pkg_name_version_req(pkg_name_version: &str) -> Result<(String, VersionReq)> {
    let parts: Vec<&str> = pkg_name_version.split('@').collect();
    if parts.len() == 2 {
        // Currently, tman uses the Rust semver crate, while the cloud store
        // uses the npm semver package. The semver requirement specifications of
        // these two packages are not completely identical. For example:
        //
        // - The Rust semver crate uses "," to separate different ranges, whereas the
        //   npm semver package uses a space (" ") to separate different requirement
        //   ranges.
        // - The npm semver package uses "||" to unify different ranges, but the Rust
        //   semver crate does not support this feature.
        //
        // Since TEN is a cross-language system, it needs to define its own
        // semver requirement specification. This specification could follow
        // either the Rust or npm format or other spec, but in either case, tman
        // or the cloud store would need to make adaptations.
        //
        // Therefore, the current approach is to simplify the specification to
        // only support a single-range semver requirement, which is the common
        // subset of both the npm semver package and the Rust semver crate.
        if parts[1].contains("||")
            || parts[1].chars().any(|c| c.is_whitespace())
            || parts[1].contains(",")
        {
            return Err(anyhow!(
                "Invalid version requirement '{}' in package name version string: contains \
                 forbidden characters (||, whitespace, or ,)",
                parts[1]
            ));
        }

        Ok((parts[0].to_string(), VersionReq::parse(parts[1])?))
    } else {
        Ok((pkg_name_version.to_string(), VersionReq::STAR))
    }
}

/// Check the latest version information, and return (update_available,
/// latest_version).
pub async fn check_update() -> Result<(bool, String), String> {
    let client = create_http_client_with_proxies()
        .map_err(|e| format!("Failed to build HTTP client: {e}"))?;

    let response = client
        .get(GITHUB_RELEASE_URL)
        .header("User-Agent", "TEN Framework Updater")
        .timeout(Duration::from_secs(GITHUB_RELEASE_REQUEST_TIMEOUT_SECS))
        .send()
        .await
        .map_err(|e| {
            if e.is_timeout() {
                "Check for updates timed out. Please try again later.".to_string()
            } else {
                format!("Failed to check for updates: {e}")
            }
        })?;

    if !response.status().is_success() {
        return Err(format!("Failed to check for updates: {}", response.status()));
    }

    let json: Value =
        response.json().await.map_err(|e| format!("Failed to parse update information: {e}"))?;
    let latest_version = json.get("tag_name").and_then(|v| v.as_str()).unwrap_or("").to_string();

    if latest_version.is_empty() {
        return Err("Failed to get the latest version information.".to_string());
    }

    let current_version = Version::parse(VERSION).unwrap_or(Version::new(0, 0, 0));
    let latest_semver = Version::parse(&latest_version).unwrap_or(Version::new(0, 0, 0));

    Ok((latest_semver > current_version, latest_version))
}
