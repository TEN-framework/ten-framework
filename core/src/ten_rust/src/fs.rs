//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fs::File,
    io::{self, Read},
    path::Path,
};

use anyhow::Result;

pub fn read_file_to_string<P: AsRef<Path>>(path: P) -> Result<String> {
    let path_ref = path.as_ref();

    let mut file = File::open(path_ref).map_err(|e| match e.kind() {
        io::ErrorKind::NotFound => {
            anyhow::anyhow!("'{}' is not found.", path_ref.to_string_lossy())
        }
        _ => anyhow::anyhow!(
            "'{}' is invalid: {}",
            path_ref.to_string_lossy(),
            e
        ),
    })?;
    let mut contents = String::new();
    file.read_to_string(&mut contents).map_err(|e| {
        anyhow::anyhow!("Failed to read content, {}.", e.to_string())
    })?;
    Ok(contents)
}
