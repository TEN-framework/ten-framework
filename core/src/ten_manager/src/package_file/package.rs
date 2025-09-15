//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;
use std::{fs::File, path::Path};

use anyhow::{Context, Result};
use flate2::{write::GzEncoder, Compression};
use tar::{Builder as TarBuilder, Header};

pub fn tar_gz_files_to_file<P: AsRef<Path>>(
    files: Vec<P>,
    prefix: &Path,
    output_tar_gz_file_path: &String,
) -> Result<()> {
    let output_file = File::create(output_tar_gz_file_path)?;
    let mut gz_encoder = GzEncoder::new(output_file, Compression::default());

    {
        let mut tar_builder = TarBuilder::new(&mut gz_encoder);

        for path in files {
            let path = path.as_ref();
            let relative_path = path.strip_prefix(prefix)?.to_path_buf();

            // Retrieve metadata without following symbolic links.
            let metadata = path.symlink_metadata()?;

            if metadata.file_type().is_symlink() {
                let target = path
                    .read_link()
                    .with_context(|| format!("Failed to read symlink target for {path:?}"))?;

                let mut header = Header::new_gnu();
                header.set_size(0); // The size of a symbolic link is 0.
                header.set_entry_type(tar::EntryType::Symlink);

                // Set the target path of a symbolic link.
                header
                    .set_link_name(&target)
                    .with_context(|| format!("Failed to set link name for {path:?}"))?;

                #[cfg(unix)]
                {
                    // Retrieve original permissions.
                    let mode = metadata.permissions().mode();
                    header.set_mode(mode);
                }
                #[cfg(not(unix))]
                {
                    // Set default permissions on Windows.
                    header.set_mode(0o777);
                }

                tar_builder
                    .append_link(&mut header, &relative_path, &target)
                    .with_context(|| format!("Failed to append symlink {path:?}"))?;
            } else if path.is_file() {
                tar_builder.append_path_with_name(path, &relative_path)?;
            } else if metadata.is_dir() {
                // If it is a directory and not the current one ".", then
                // package the directory.
                if !relative_path.as_os_str().is_empty() && relative_path != Path::new(".") {
                    tar_builder.append_dir_all(&relative_path, path)?;
                }
            }
        }

        // Finish the tar archive and retrieve the GzEncoder.
        tar_builder.finish()?;
    }

    // Properly finalize the GzEncoder.
    gz_encoder.finish()?;

    Ok(())
}
