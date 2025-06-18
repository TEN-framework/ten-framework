//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::path::Path;

use anyhow::Result;
use url::Url;

/// Normalize a path by resolving '.' and '..' components.
/// This function manually processes path components without requiring the path
/// to exist on disk.
fn normalize_path_components(path: &Path) -> Result<String> {
    let mut components = Vec::new();

    // Handle absolute vs relative paths
    if path.is_absolute() {
        // For absolute paths, we need to preserve the root
        components.push("".to_string()); // This represents the root "/"
    }

    // Process each component
    for component in path.components() {
        match component {
            std::path::Component::Prefix(_) => {
                // Windows drive letters, etc. - preserve as-is
                components
                    .push(component.as_os_str().to_string_lossy().to_string());
            }
            std::path::Component::RootDir => {
                // Root directory - already handled above for absolute paths
                continue;
            }
            std::path::Component::CurDir => {
                // Current directory "." - skip it
                continue;
            }
            std::path::Component::ParentDir => {
                // Parent directory ".."
                if components.len() > 1
                    || (components.len() == 1 && components[0] != "")
                {
                    // We can go up one level
                    components.pop();
                } else if components.is_empty() {
                    // Relative path, add ".."
                    components.push("..".to_string());
                }
                // If we're already at root (components has single empty
                // element), ignore ".."
            }
            std::path::Component::Normal(name) => {
                // Normal path component
                components.push(name.to_string_lossy().to_string());
            }
        }
    }

    if components.is_empty() {
        return Ok(".".to_string());
    }

    // Reconstruct the path
    if path.is_absolute() {
        if components.len() == 1 && components[0] == "" {
            // Root directory only
            Ok("/".to_string())
        } else {
            // Remove the empty root component and join with "/"
            let path_components: Vec<&str> =
                components.iter().skip(1).map(|s| s.as_str()).collect();
            if path_components.is_empty() {
                Ok("/".to_string())
            } else {
                Ok(format!("/{}", path_components.join("/")))
            }
        }
    } else {
        Ok(components.join("/"))
    }
}

/// Get the real path of the interface according to the import_uri and base_dir.
///
/// The real path is the path of the interface file.
///
/// The import_uri can be a relative path or a URL.
/// The base_dir is the base directory of the interface file.
pub fn get_real_path_from_import_uri(
    import_uri: &str,
    base_dir: &str,
) -> Result<String> {
    // First check if the import_uri is an absolute path - these are not
    // supported. We need to check for both Unix and Windows absolute paths.
    let is_absolute = Path::new(import_uri).is_absolute()
        || (import_uri.len() >= 3
            && import_uri.chars().nth(1) == Some(':')
            && import_uri.chars().nth(2) == Some('\\'));

    if is_absolute {
        return Err(anyhow::anyhow!(
            "Absolute paths are not supported in import_uri: {}. Use file:// \
             URI or relative path instead",
            import_uri
        ));
    }

    // Try to parse as URL. If it's a URL, the base_dir is not used.
    if let Ok(url) = Url::parse(import_uri) {
        match url.scheme() {
            "http" | "https" => {
                return Ok(url.to_string());
            }
            "file" => {
                return Ok(url.to_string());
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

    // If it's not a URL, it's a relative path based on the base_dir.

    // If the base_dir is not provided, return an error.
    if base_dir.is_empty() {
        return Err(anyhow::anyhow!(
            "Base directory is not provided in import_uri: {}",
            import_uri
        ));
    }

    // If the base_dir is a URL, calculate the real path based on the URL.
    // For example, if the base_dir is "http://localhost:8080/api/v1" and
    // the import_uri is "interface.json", the real path is
    // "http://localhost:8080/api/v1/interface.json".
    // If the base_dir is "file:///home/user/tmp" and the import_uri is
    // "../interface.json", the real path is "file:///home/user/interface.json".
    if let Ok(mut base_url) = Url::parse(base_dir) {
        // Check if it's a real URL scheme (not just a Windows path with a
        // colon)
        if base_url.scheme().len() > 1
            && !base_url.scheme().eq_ignore_ascii_case("c")
        {
            // Ensure the base URL ends with '/' to properly append relative
            // paths
            if !base_url.path().ends_with('/') {
                base_url.set_path(&format!("{}/", base_url.path()));
            }

            // Use URL's join method to properly handle relative paths
            match base_url.join(import_uri) {
                Ok(resolved_url) => {
                    // Canonicalize the path to resolve . and .. components

                    return Ok(resolved_url.to_string());
                }
                Err(e) => {
                    return Err(anyhow::anyhow!(
                        "Failed to resolve relative path '{}' against base \
                         URL '{}': {}",
                        import_uri,
                        base_dir,
                        e
                    ));
                }
            }
        }
    }

    // If the base_dir is not a URL, it's a relative path.
    let path = Path::new(base_dir).join(import_uri);

    // Normalize the path to resolve '.' and '..' components
    normalize_path_components(&path)
}
