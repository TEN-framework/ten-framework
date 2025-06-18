//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::{collections::HashSet, path::Path};

use crate::{
    fs::read_file_to_string,
    path::get_real_path_from_import_uri,
    pkg_info::manifest::api::{ManifestApi, ManifestApiInterface},
};

use anyhow::{anyhow, Context, Result};
use url::Url;

async fn load_interface_from_http_url_async(url: &Url) -> Result<ManifestApi> {
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
        return Err(anyhow::anyhow!(
            "HTTP request failed with status {}: {}",
            response.status(),
            url
        ));
    }

    // Get response body as text
    let interface_content = response
        .text()
        .await
        .with_context(|| format!("Failed to read response body from {url}"))?;

    // Parse the interface file into a ManifestApi structure.
    let mut interface_api: ManifestApi =
        serde_json::from_str(&interface_content).with_context(|| {
            format!("Failed to parse interface file from {url}")
        })?;

    // Set the base_dir of the interface.
    if let Some(interface) = &mut interface_api.interface.as_mut() {
        let mut base_url = url.clone();
        // Remove the file part from the URL to get the base directory
        if let Ok(mut segments) = base_url.path_segments_mut() {
            segments.pop();
        }

        for interface in interface.iter_mut() {
            interface.base_dir = base_url.to_string();
        }
    }

    Ok(interface_api)
}

fn load_interface_from_http_url(url: &Url) -> Result<ManifestApi> {
    let rt = tokio::runtime::Runtime::new()
        .context("Failed to create tokio runtime")?;

    rt.block_on(load_interface_from_http_url_async(url))
}

fn load_interface_from_file_url(url: &Url) -> Result<ManifestApi> {
    // Convert file URL to local path
    let path =
        url.to_file_path().map_err(|_| anyhow!("Invalid file URL: {}", url))?;

    // Read the interface file.
    let interface_content = read_file_to_string(&path).with_context(|| {
        format!("Failed to read interface file from {}", path.display())
    })?;

    // Parse the interface file into a ManifestApi structure.
    let mut interface_api: ManifestApi =
        serde_json::from_str(&interface_content).with_context(|| {
            format!("Failed to parse interface file from {}", path.display())
        })?;

    // Set the base_dir of the interface.
    if let Some(interface) = &mut interface_api.interface.as_mut() {
        let mut base_url = url.clone();
        // Remove the file part from the URL to get the base directory
        if let Ok(mut segments) = base_url.path_segments_mut() {
            segments.pop();
        }

        for interface in interface.iter_mut() {
            interface.base_dir = base_url.to_string();
        }
    }

    Ok(interface_api)
}

/// Loads interface from the specified URI with an optional base directory.
///
/// The URI can be:
/// - A relative path (relative to the base_dir if provided)
/// - A URI (http:// or https:// or file://)
///
/// If the interface is already loaded or cannot be loaded, return an error.
pub fn load_interface(
    interface: &ManifestApiInterface,
    interface_set: &mut HashSet<String>,
) -> Result<ManifestApi> {
    let import_uri = &interface.import_uri;
    let base_dir = &interface.base_dir;

    // Get the real path according to the import_uri and base_dir.
    let real_path = get_real_path_from_import_uri(import_uri, base_dir)?;

    // Check if the interface is in the interface_set.
    if interface_set.contains(&real_path) {
        return Err(anyhow::anyhow!(
            "Circular reference detected: {}",
            real_path
        ));
    }

    // Add the interface to the interface_set.
    interface_set.insert(real_path.clone());

    // Try to parse as URL
    if let Ok(url) = Url::parse(&real_path) {
        match url.scheme() {
            "http" | "https" => {
                return load_interface_from_http_url(&url);
            }
            "file" => {
                return load_interface_from_file_url(&url);
            }
            _ => {
                return Err(anyhow::anyhow!(
                    "Unsupported URL scheme '{}' in import_uri: {}",
                    url.scheme(),
                    import_uri
                ));
            }
        }
    }

    // It's a file path, read the interface file.
    let interface_content =
        read_file_to_string(&real_path).with_context(|| {
            format!("Failed to read interface file from {}", real_path)
        })?;

    // Parse the interface file into a ManifestApi structure.
    let mut interface_api: ManifestApi =
        serde_json::from_str(&interface_content).with_context(|| {
            format!("Failed to parse interface file from {}", real_path)
        })?;

    // Get the parent directory of the interface file.
    let parent_dir = Path::new(&real_path).parent().unwrap();

    // Set the base_dir of the interface.
    if let Some(interface) = &mut interface_api.interface {
        for interface in interface.iter_mut() {
            interface.base_dir = parent_dir.to_string_lossy().to_string();
        }
    }

    Ok(interface_api)
}

impl ManifestApi {
    /// Helper function that contains the common logic for flattening a
    /// ManifestApi instance.
    fn flatten_internal<F>(
        &self,
        interface_loader: &F,
        flattened_apis: &mut Vec<ManifestApi>,
        interface_set: &mut HashSet<String>,
    ) -> Result<()>
    where
        F: Fn(
            &ManifestApiInterface,
            &mut HashSet<String>,
        ) -> Result<ManifestApi>,
    {
        // Push the current ManifestApi to the flattened_apis.
        flattened_apis.push(self.clone());

        // Check if the ManifestApi contains any interface.
        let has_interfaces = self.interface.is_some()
            && !self.interface.as_ref().unwrap().is_empty();

        if !has_interfaces {
            // No interfaces, return immediately.
            return Ok(());
        }

        // This ManifestApi has interfaces, so we need to flatten them.
        for interface in self.interface.as_ref().unwrap() {
            // Load the interface.
            // If the interface is already loaded or cannot be loaded,
            // return an error.
            let loaded_interface = interface_loader(interface, interface_set)?;

            // Flatten the loaded interface.
            loaded_interface.flatten_internal(
                interface_loader,
                flattened_apis,
                interface_set,
            )?;
        }

        Ok(())
    }

    /// Convenience method for flattening a ManifestApi instance.
    ///
    /// Returns `Ok(None)` if the ManifestApi contains no interface and doesn't
    /// need flattening. Returns `Ok(Some(flattened_manifest_api))` if the
    /// ManifestApi was successfully flattened.
    pub fn flatten<F>(
        &self,
        interface_loader: &F,
    ) -> Result<Option<ManifestApi>>
    where
        F: Fn(
            &ManifestApiInterface,
            &mut HashSet<String>,
        ) -> Result<ManifestApi>,
    {
        // Check if the ManifestApi contains any interface.
        if self.interface.is_none()
            || self.interface.as_ref().unwrap().is_empty()
        {
            return Ok(None);
        }

        // This ManifestApi has interfaces, so we need to flatten them.
        let mut flattened_apis = Vec::new();
        let mut interface_set = HashSet::new();

        self.flatten_internal(
            interface_loader,
            &mut flattened_apis,
            &mut interface_set,
        )?;

        // Merge the flattened apis into a single ManifestApi.
        Err(anyhow::anyhow!("Not implemented"))
    }
}
