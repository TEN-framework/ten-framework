//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::path::{Component, Path, PathBuf};

use anyhow::Result;
use url::Url;

pub fn normalize_path(path: &Path) -> PathBuf {
    let mut components = path.components().peekable();
    let mut ret =
        if let Some(c @ Component::Prefix(..)) = components.peek().cloned() {
            components.next();
            PathBuf::from(c.as_os_str())
        } else {
            PathBuf::new()
        };

    for component in components {
        match component {
            Component::Prefix(..) => unreachable!(),
            Component::RootDir => {
                ret.push(component.as_os_str());
            }
            Component::CurDir => {}
            Component::ParentDir => {
                ret.pop();
            }
            Component::Normal(c) => {
                ret.push(c);
            }
        }
    }
    ret
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
    if Path::new(import_uri).is_absolute() {
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
                #[cfg(windows)]
                // Windows drive letter
                if url.scheme().len() == 1
                    && url
                        .scheme()
                        .chars()
                        .next()
                        .unwrap()
                        .is_ascii_alphabetic()
                {
                    // The import_uri may be a relative path in Windows.
                    // Continue to parse the import_uri as a relative path.
                } else {
                    return Err(anyhow::anyhow!(
                        "Unsupported URL scheme '{}' in import_uri: {} when \
                         get_real_path_from_import_uri",
                        url.scheme(),
                        import_uri
                    ));
                }

                #[cfg(not(windows))]
                return Err(anyhow::anyhow!(
                    "Unsupported URL scheme '{}' in import_uri: {} when \
                     get_real_path_from_import_uri",
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
    Ok(normalize_path(&path).to_string_lossy().to_string())
}
