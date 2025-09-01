//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(unix)]
use std::os::unix::fs::symlink;
#[cfg(windows)]
use std::os::windows::fs::{symlink_dir, symlink_file};
use std::{
    fs::{self, File},
    io::{self, BufWriter},
    path::Path,
};

use anyhow::{anyhow, Context, Result};
use flate2::read::GzDecoder;
use tar::Archive as TarArchive;
use zip::ZipArchive;

use crate::{
    constants::BUF_WRITER_BUF_SIZE,
    fs::file_type::detect_file_type,
    install::{
        installed_paths::{sort_installed_paths, InstalledPaths},
        template::{
            extract_and_process_tar_gz_template_part, extract_and_process_zip_template_part,
        },
    },
};

fn create_symlink(target: &str, link: &Path, output_dir: &str) -> Result<()> {
    #[cfg(unix)]
    {
        // Dummy usage to silence Clippy on non-Windows platforms.
        let _ = output_dir;

        symlink(target, link)
            .with_context(|| format!("Failed to create symlink {link:?} -> {target:?}"))?;
    }

    #[cfg(windows)]
    {
        // Determine the absolute path of the target.
        let target_path = Path::new(output_dir).join(target);
        let target_metadata = fs::metadata(&target_path)
            .with_context(|| format!("Failed to get metadata for target {:?}", target_path))?;

        if target_metadata.is_dir() {
            symlink_dir(target, link).with_context(|| {
                format!("Failed to create directory symlink {:?} -> {:?}", link, target)
            })?;
        } else {
            symlink_file(target, link).with_context(|| {
                format!("Failed to create file symlink {:?} -> {:?}", link, target)
            })?;
        }
    }

    Ok(())
}

fn extract_and_process_zip_normal_part(zip_path: &str, output_dir: &str) -> Result<InstalledPaths> {
    // Open the ZIP file.
    let file = File::open(zip_path)?;
    let mut archive = ZipArchive::new(file)?;

    let mut installed_paths = InstalledPaths {
        paths: Vec::new(),
    };

    // Iterate through each entry in the ZIP.
    for i in 0..archive.len() {
        let mut file = archive.by_index(i)?;
        let is_template = file.name().ends_with(".tent");
        if is_template {
            continue;
        }

        let out_path = Path::new(output_dir).join(file.mangled_name());

        // Check if the entry is a file or directory.
        if file.name().ends_with('/') {
            fs::create_dir_all(&out_path)?;
        } else {
            // Ensure that the parent directory exists.
            if let Some(p) = out_path.parent() {
                if !p.exists() {
                    fs::create_dir_all(p)?;
                }
            }

            let out_file = File::create(&out_path)?;
            let mut buffered_writer = BufWriter::with_capacity(BUF_WRITER_BUF_SIZE, out_file);
            io::copy(&mut file, &mut buffered_writer)?;

            #[cfg(unix)]
            {
                // Set file permissions if it's a Unix system.
                use std::os::unix::fs::PermissionsExt;

                // Set file permissions if it's a Unix system.
                if let Some(mode) = file.unix_mode() {
                    fs::set_permissions(&out_path, fs::Permissions::from_mode(mode))?;
                }
            }
        }

        let relative_path = out_path.strip_prefix(output_dir)?.to_str().unwrap().to_string();
        installed_paths.paths.push(relative_path);
    }

    Ok(installed_paths)
}

fn extract_and_process_zip(
    zip_path: &str,
    output_dir: &str,
    template_ctx: Option<&serde_json::Value>,
) -> Result<InstalledPaths> {
    let mut installed_paths = extract_and_process_zip_normal_part(zip_path, output_dir)?;

    if let Some(template_ctx) = template_ctx {
        // In the 'template' mode.
        extract_and_process_zip_template_part(zip_path, output_dir, template_ctx)?;

        let rtet_paths: Vec<_> =
            installed_paths.paths.iter().filter(|p| p.ends_with(".tent")).cloned().collect();

        for rtet_path in rtet_paths {
            installed_paths.paths.retain(|p| p != &rtet_path);

            let original_path = rtet_path.trim_end_matches(".tent").to_string();
            if !installed_paths.paths.contains(&original_path) {
                installed_paths.paths.push(original_path);
            }
        }
    }

    // Custom sort: directories after their contents.
    sort_installed_paths(&mut installed_paths);

    Ok(installed_paths)
}

fn extract_and_process_tar_gz_normal_part(
    tar_gz_path: &str,
    output_dir: &str,
) -> Result<InstalledPaths> {
    // Open the package file.
    let file = File::open(tar_gz_path)?;
    let mut archive = TarArchive::new(GzDecoder::new(file));

    let mut installed_paths = InstalledPaths {
        paths: Vec::new(),
    };

    // Iterate through each entry in the package file.
    for entry_result in archive.entries()? {
        let mut entry = entry_result?;
        let entry_path = entry.path()?;
        let entry_str = entry_path.to_string_lossy().to_string();

        let is_template = entry_str.ends_with(".tent");
        if is_template {
            // Do not process template files while handling the normal part;
            // leave them to be handled separately when specifically processing
            // the template part.
            continue;
        }

        let out_path = Path::new(output_dir).join(&entry_str);

        if entry.header().entry_type().is_symlink() {
            // Recreate the symbolic link.
            let target = entry
                .link_name()
                .with_context(|| format!("Symlink entry {entry_path:?} does not have a target"))?
                .ok_or_else(|| anyhow!("Symlink entry {:?} does not have a target", entry_path))?
                .to_string_lossy()
                .to_string();

            // Ensure the parent directory exists.
            if let Some(p) = out_path.parent() {
                fs::create_dir_all(p).with_context(|| {
                    format!("Failed to create parent directory for {out_path:?}")
                })?;
            }

            if out_path.exists() {
                fs::remove_file(&out_path).with_context(|| {
                    format!(
                        "Failed to remove existing file at {out_path:?} before creating symlink"
                    )
                })?;
            }

            // Create the symbolic link.
            create_symlink(&target, &out_path, output_dir)?;
        }
        // Check if the entry is a file or directory.
        else if entry.header().entry_type().is_dir() {
            fs::create_dir_all(&out_path)?;
        } else {
            // Ensure that the parent directory exists.
            if let Some(p) = out_path.parent() {
                if !p.exists() {
                    fs::create_dir_all(p)?;
                }
            }

            let out_file = File::create(&out_path)?;
            let mut buffered_writer = BufWriter::with_capacity(BUF_WRITER_BUF_SIZE, out_file);
            io::copy(&mut entry, &mut buffered_writer)?;

            #[cfg(unix)]
            {
                // Set file permissions if it's a Unix system.
                use std::os::unix::fs::PermissionsExt;

                // Set file permissions if it's a Unix system.
                if let Ok(mode) = entry.header().mode() {
                    fs::set_permissions(&out_path, fs::Permissions::from_mode(mode))?;
                }
            }
        }

        let relative_path = out_path.strip_prefix(output_dir)?.to_str().unwrap().to_string();
        installed_paths.paths.push(relative_path);
    }

    Ok(installed_paths)
}

fn extract_and_process_tar_gz(
    tar_gz_path: &str,
    output_dir: &str,
    template_ctx: Option<&serde_json::Value>,
) -> Result<InstalledPaths> {
    let mut installed_paths = extract_and_process_tar_gz_normal_part(tar_gz_path, output_dir)?;

    if let Some(template_ctx) = template_ctx {
        // In the 'template' mode.
        extract_and_process_tar_gz_template_part(tar_gz_path, output_dir, template_ctx)?;

        let tent_paths: Vec<_> =
            installed_paths.paths.iter().filter(|p| p.ends_with(".tent")).cloned().collect();

        for tent_path in tent_paths {
            installed_paths.paths.retain(|p| p != &tent_path);

            let original_path = tent_path.trim_end_matches(".tent").to_string();
            if !installed_paths.paths.contains(&original_path) {
                installed_paths.paths.push(original_path);
            }
        }
    }

    // Custom sort: directories after their contents.
    sort_installed_paths(&mut installed_paths);

    Ok(installed_paths)
}

pub fn extract_and_process_tpkg_file(
    tpkg_path: &Path,
    output_dir: &str,
    template_ctx: Option<&serde_json::Value>,
) -> Result<InstalledPaths> {
    let installed_paths = match detect_file_type(tpkg_path)? {
        crate::fs::file_type::FileType::TarGz => {
            extract_and_process_tar_gz(&tpkg_path.to_string_lossy(), output_dir, template_ctx)?
        }
        crate::fs::file_type::FileType::Zip => {
            extract_and_process_zip(&tpkg_path.to_string_lossy(), output_dir, template_ctx)?
        }
    };

    Ok(installed_paths)
}
