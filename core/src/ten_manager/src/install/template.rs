//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::path::Path;

use anyhow::{Context, Result};
use flate2::read::GzDecoder;
use handlebars::Handlebars;
use tar::Archive as TarArchive;
use zip::ZipArchive;

fn render_template(
    template: &str,
    template_ctx: &serde_json::Value,
) -> Result<String> {
    let reg = Handlebars::new();
    let rendered = reg
        .render_template(template, template_ctx)
        .with_context(|| "Failed to render template")?;
    Ok(rendered)
}

fn handle_tent_file<F>(
    out_path: &Path,
    is_dir: bool,
    file_mode: Option<u32>,
    read_file_content: F,
    template_ctx: &serde_json::Value,
) -> Result<()>
where
    // Callback: how to write data from the archive to `out_path`.
    F: FnOnce(&mut File) -> io::Result<()>,
{
    if is_dir {
        fs::create_dir_all(out_path)?;
    } else {
        if let Some(parent) = out_path.parent() {
            if !parent.exists() {
                fs::create_dir_all(parent)?;
            }
        }

        // Copy the data from the archive to `out_path`.
        let mut out_file = File::create(out_path)?;
        read_file_content(&mut out_file)?;
    }

    // If it is a directory, return directly; directories do not have subsequent
    // reading/rendering processes.
    if is_dir {
        return Ok(());
    }

    // Read the text content of the `*.tent` file for rendering.
    let mut contents = String::new();
    {
        let mut out_file_for_read = File::open(out_path)?;
        out_file_for_read.read_to_string(&mut contents)?;
    }
    let rendered = render_template(&contents, template_ctx)?;

    // Remove the `.tent` suffix.
    let new_out_path = out_path.with_extension("");

    // Write the template rendering result to a new file path.
    let mut new_out_file = File::create(&new_out_path)?;
    new_out_file.write_all(rendered.as_bytes())?;

    // If it is a Unix system and permissions are successfully retrieved from
    // the archive, set them.
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        if let Some(mode) = file_mode {
            fs::set_permissions(
                &new_out_path,
                fs::Permissions::from_mode(mode),
            )?;
        }
    }

    // Delete the original `*.tent` file.
    fs::remove_file(out_path)?;

    Ok(())
}

pub fn extract_and_process_zip_template_part(
    zip_path: &str,
    output_dir: &str,
    template_ctx: &serde_json::Value,
) -> Result<()> {
    // Open the ZIP file.
    let file = File::open(zip_path)?;
    let mut archive = ZipArchive::new(file)?;

    // Iterate through each entry in the ZIP.
    for i in 0..archive.len() {
        let mut file_in_zip = archive.by_index(i)?;
        let is_template = file_in_zip.name().ends_with(".tent");

        if is_template {
            let out_path =
                Path::new(output_dir).join(file_in_zip.mangled_name());

            // Check if the entry is a file or directory.
            let is_dir = file_in_zip.name().ends_with('/');

            // Retrieve the permission mode on Unix (if it exists).
            #[cfg(unix)]
            let file_mode = file_in_zip.unix_mode();
            #[cfg(not(unix))]
            let file_mode = None;

            handle_tent_file(
                &out_path,
                is_dir,
                file_mode,
                // Callback: how to copy the contents from a ZIP file to the
                // target file.
                |out_file: &mut File| {
                    if !is_dir {
                        // If it is a file, copy it.
                        io::copy(&mut file_in_zip, out_file)?;
                    }
                    Ok(())
                },
                template_ctx,
            )?;
        }
    }

    Ok(())
}

pub fn extract_and_process_tar_gz_template_part(
    pkg_path: &str,
    output_dir: &str,
    template_ctx: &serde_json::Value,
) -> Result<()> {
    let file = File::open(pkg_path)?;
    let mut archive = TarArchive::new(GzDecoder::new(file));

    // Iterate through each entry in the package file.
    for entry_result in archive.entries()? {
        let mut entry = entry_result?;
        let entry_path = entry.path()?;
        let entry_str = entry_path.to_string_lossy().to_string();

        let is_template = entry_str.ends_with(".tent");
        if is_template {
            // Note that we set the target file path to be written as
            // `output_dir + entry_str`, ensuring that the final relative path
            // written locally matches the path inside the tar file.
            let out_path = Path::new(output_dir).join(&entry_str);

            // Check if the entry is a file or directory.
            let is_dir = entry.header().entry_type().is_dir();

            // Retrieve the permission mode on Unix (if it exists).
            #[cfg(unix)]
            let file_mode = entry.header().mode().ok();
            #[cfg(not(unix))]
            let file_mode = None;

            handle_tent_file(
                &out_path,
                is_dir,
                file_mode,
                // Callback: how to copy the contents from a tar.gz file to the
                // target file.
                |out_file: &mut File| {
                    if !is_dir {
                        io::copy(&mut entry, out_file)?;
                    }
                    Ok(())
                },
                template_ctx,
            )?;
        }
    }

    Ok(())
}
