//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fs;
use std::path::Path;
use std::sync::Arc;

use actix_web::{
    error::{ErrorForbidden, ErrorNotFound},
    web, HttpResponse, Responder,
};
use serde::{Deserialize, Serialize};

use super::{
    response::{ApiResponse, Status},
    DesignerState,
};

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct FsEntry {
    pub name: String,
    pub path: String,
    pub is_dir: bool,
}

#[derive(Deserialize)]
pub struct ListDirRequestPayload {
    path: String,
}

#[derive(Serialize)]
pub struct DirListResponseData {
    pub entries: Vec<FsEntry>,
}

pub async fn list_dir_endpoint(
    request_payload: web::Json<ListDirRequestPayload>,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let path = Path::new(&request_payload.path);

    // Check if path exists.
    if !path.exists() {
        return Err(ErrorNotFound("Path does not exist"));
    }

    // Check path permissions.
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        if let Ok(metadata) = fs::metadata(path) {
            let mode = metadata.mode();
            if mode & 0o400 == 0 {
                // Check read permission.
                return Err(ErrorForbidden("Permission denied"));
            }
        }
    }

    let mut entries = Vec::new();

    // Case 1: path is a file.
    if path.is_file() {
        let file_name = path
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string();
        entries.push(FsEntry {
            name: file_name,
            path: request_payload.path.clone(),
            is_dir: false,
        });
    }
    // Case 2: path is a directory.
    else if path.is_dir() {
        if let Ok(dir_entries) = fs::read_dir(path) {
            for entry in dir_entries.flatten() {
                let is_dir = if let Ok(file_type) = entry.file_type() {
                    file_type.is_dir()
                } else {
                    false
                };
                let name = entry.file_name().to_string_lossy().to_string();
                let entry_path = entry.path().to_string_lossy().to_string();
                entries.push(FsEntry {
                    name,
                    path: entry_path,
                    is_dir,
                });
            }
        }
    }

    let response = ApiResponse {
        status: Status::Ok,
        data: DirListResponseData { entries },
        meta: None,
    };
    Ok(HttpResponse::Ok().json(response))
}
