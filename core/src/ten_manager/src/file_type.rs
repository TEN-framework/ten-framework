//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fs::File;
use std::io::Read;
use std::path::Path;

use anyhow::{anyhow, Result};

#[derive(Debug, PartialEq)]
pub enum FileType {
    Zip,
    TarGz,
}

/// Manual file type detection.
fn manual_detect<P: AsRef<Path>>(path: P) -> Result<FileType> {
    let mut file = File::open(path)?;
    let mut buffer = [0u8; 4];

    let bytes_read = file.read(&mut buffer)?;

    if bytes_read >= 4 && &buffer[0..4] == b"PK\x03\x04" {
        return Ok(FileType::Zip);
    }

    if bytes_read >= 2 && buffer[0] == 0x1F && buffer[1] == 0x8B {
        return Ok(FileType::TarGz);
    }

    Err(anyhow!("Failed to detect file type."))
}

/// Detect file type using the `infer` library.
fn infer_detect<P: AsRef<Path>>(path: P) -> Result<FileType> {
    let mut file = File::open(path)?;
    let mut buffer = Vec::new();
    file.read_to_end(&mut buffer)?;

    if let Some(kind) = infer::get(&buffer) {
        match kind.mime_type() {
            "application/zip" => return Ok(FileType::Zip),
            "application/gzip" => return Ok(FileType::TarGz),
            _ => {}
        }
    }

    Err(anyhow!("Failed to detect file type."))
}

pub fn detect_file_type<P: AsRef<Path>>(path: P) -> Result<FileType> {
    // Use `infer` library to do the detection first.
    if let Ok(file_type) = infer_detect(&path) {
        return Ok(file_type);
    }

    // If `infer` detection fails, use the manual method.
    manual_detect(path)
}
