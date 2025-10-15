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
    let mut ret = if let Some(c @ Component::Prefix(..)) = components.peek().cloned() {
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

/// Sanitize the local path to make it a valid Windows path.
/// It will strip the Windows verbatim prefix (e.g., \\?\ or \\?\UNC\) and
/// replace the '/' with '\'.
#[cfg(windows)]
fn sanitize_windows_local_path(raw_path: Option<&str>) -> Option<String> {
    raw_path.map(|path| {
        if let Some(rest) = path.strip_prefix("\\\\?\\UNC\\") {
            format!("\\\\{}", rest).replace('\\', "/")
        } else if let Some(rest) = path.strip_prefix("\\\\?\\") {
            rest.replace('\\', "/")
        } else {
            path.replace('\\', "/")
        }
    })
}

pub fn get_base_dir_of_uri(uri: &str) -> Result<String> {
    if let Ok(url) = Url::parse(uri) {
        match url.scheme() {
            "http" | "https" | "file" => {
                let mut base_url = url.clone();

                // Remove the file part from the URL to get the base directory
                if let Ok(mut segments) = base_url.path_segments_mut() {
                    segments.pop();
                }

                return Ok(base_url.to_string());
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
                        "Unsupported URL scheme '{}' in uri: {} when get_base_dir_of_uri",
                        url.scheme(),
                        uri
                    ));
                }

                #[cfg(not(windows))]
                return Err(anyhow::anyhow!(
                    "Unsupported URL scheme '{}' in uri: {} when get_base_dir_of_uri",
                    url.scheme(),
                    uri
                ));
            }
        }
    }

    // It's a relative path, return the parent directory.
    let parent_dir = Path::new(uri).parent().unwrap();

    // It's a file path, return the parent directory.
    Ok(parent_dir.to_string_lossy().to_string())
}

/// Get the real path of the import_uri based on the base_dir.
///
/// The import_uri can be a relative path or a URL.
/// The base_dir is the base directory of the import_uri if it's a relative
/// path.
/// If import_uri contains ${app_base_dir}, it will be replaced with the
/// app_base_dir parameter.
pub fn get_real_path_from_import_uri(
    import_uri: &str,
    raw_base_dir: Option<&str>,
    raw_app_base_dir: Option<&str>,
) -> Result<String> {
    // If the import_uri is an absolute path (without variable substitution),
    // return an error because absolute paths should use file:// URI
    if Path::new(import_uri).is_absolute() && !import_uri.contains("${app_base_dir}") {
        return Err(anyhow::anyhow!(
            "Absolute paths are not supported in import_uri: {}. Use file:// URI or relative path \
             instead",
            import_uri
        ));
    }

    // Sanitize path (only on Windows).
    // Remove the Windows verbatim prefix (e.g., \\?\ or \\?\UNC\) and
    // replace the '\' with '/'.
    let base_dir: Option<&str>;
    #[cfg(windows)]
    let base_dir_string: Option<String>;
    let app_base_dir: Option<&str>;
    #[cfg(windows)]
    let app_base_dir_string: Option<String>;

    #[cfg(windows)]
    {
        base_dir_string = sanitize_windows_local_path(raw_base_dir);
        base_dir = base_dir_string.as_deref();
        app_base_dir_string = sanitize_windows_local_path(raw_app_base_dir);
        app_base_dir = app_base_dir_string.as_deref();
    }
    #[cfg(not(windows))]
    {
        base_dir = raw_base_dir;
        app_base_dir = raw_app_base_dir;
    }

    // Check if import_uri contains ${app_base_dir} variable
    let processed_import_uri = if import_uri.contains("${app_base_dir}") {
        assert!(
            import_uri.starts_with("${app_base_dir}"),
            "app_base_dir should be at the beginning of the import_uri: {}",
            import_uri
        );
        if let Some(app_base_dir) = app_base_dir {
            // Replace ${app_base_dir} with the actual app base directory
            import_uri.replace("${app_base_dir}", app_base_dir)
        } else {
            return Err(anyhow::anyhow!(
                "app_base_dir must be provided when import_uri contains ${{app_base_dir}} \
                 variable: {}",
                import_uri
            ));
        }
    } else {
        import_uri.to_string()
    };

    // If the processed import_uri is now an absolute path (after variable
    // replacement), we need to handle it differently. Also check for Windows
    // absolute paths.
    let is_absolute = Path::new(&processed_import_uri).is_absolute()
        || (processed_import_uri.len() >= 3
            && processed_import_uri.chars().nth(1) == Some(':')
            && processed_import_uri.chars().next().unwrap().is_ascii_alphabetic());

    if is_absolute {
        // Normalize the path to resolve '.' and '..' components
        let normalized_path = normalize_path(Path::new(&processed_import_uri));
        // If it's absolute after variable replacement, convert to file:// URI
        return Ok(format!("file://{}", normalized_path.to_string_lossy()));
    }

    // Try to parse as URL. If it's a URL, the base_dir is not used.
    if let Ok(url) = Url::parse(&processed_import_uri) {
        match url.scheme() {
            "http" | "https" => {
                return Ok(url.to_string());
            }
            "file" => {
                return Ok(url.to_string());
            }
            _ => {
                // Windows drive letter - Check if it's a single character and alphabetic
                if url.scheme().len() == 1
                    && url.scheme().chars().next().unwrap().is_ascii_alphabetic()
                {
                    // The processed_import_uri may be a Windows path that was
                    // incorrectly parsed as URL.
                    // Continue to parse the processed_import_uri as a file
                    // path.
                } else {
                    return Err(anyhow::anyhow!(
                        "Unsupported URL scheme '{}' in import_uri: {} when \
                         get_real_path_from_import_uri",
                        url.scheme(),
                        processed_import_uri
                    ));
                }
            }
        }
    }

    // If it's not a URL, it's a relative path based on the base_dir.

    // If the base_dir is not provided, return an error.
    if base_dir.is_none() || base_dir.unwrap().is_empty() {
        return Err(anyhow::anyhow!(
            "base_dir cannot be None when uri is a relative path, import_uri: \
             {processed_import_uri}"
        ));
    }

    // If the base_dir is a URL, calculate the real path based on the URL.
    // For example, if the base_dir is "http://localhost:8080/api/v1" and
    // the import_uri is "interface.json", the real path is
    // "http://localhost:8080/api/v1/interface.json".
    // If the base_dir is "file:///home/user/tmp" and the import_uri is
    // "../interface.json", the real path is "file:///home/user/interface.json".
    if let Ok(mut base_url) = Url::parse(base_dir.unwrap()) {
        // Check if it's a real URL scheme (not just a Windows path with a
        // colon)
        if base_url.scheme().len() > 1 && !base_url.scheme().eq_ignore_ascii_case("c") {
            // Ensure the base URL ends with '/' to properly append relative
            // paths
            if !base_url.path().ends_with('/') {
                base_url.set_path(&format!("{}/", base_url.path()));
            }

            // Use URL's join method to properly handle relative paths
            match base_url.join(&processed_import_uri) {
                Ok(resolved_url) => {
                    // Canonicalize the path to resolve . and .. components

                    return Ok(resolved_url.to_string());
                }
                Err(e) => {
                    return Err(anyhow::anyhow!(
                        "Failed to resolve relative path '{}' against base URL '{}': {}",
                        processed_import_uri,
                        base_dir.unwrap(),
                        e
                    ));
                }
            }
        }
    }

    // If the base_dir is not a URL, it's a relative path.
    let path = Path::new(base_dir.unwrap()).join(&processed_import_uri);

    // Normalize the path to resolve '.' and '..' components
    Ok(normalize_path(&path).to_string_lossy().to_string())
}
