//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fs,
    path::{Path, PathBuf},
    sync::Arc,
};

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use super::{
    response::{ApiResponse, Status},
    DesignerState,
};

/// Canonicalizes `path` and checks that the result is contained within one
/// of the canonicalized `allowed_roots`. This confines the file-content API
/// to the directories of apps that are actually loaded, so a client cannot
/// escape via an absolute path or a `..` traversal.
fn ensure_within_allowed_roots(path: &Path, allowed_roots: &[String]) -> Result<PathBuf, String> {
    if allowed_roots.is_empty() {
        return Err("No project root is currently loaded, so no file path can be validated."
            .to_string());
    }

    let canonical_path = fs::canonicalize(path)
        .map_err(|e| format!("Failed to resolve path {}: {}", path.display(), e))?;

    let is_allowed = allowed_roots
        .iter()
        .filter_map(|root| fs::canonicalize(root).ok())
        .any(|canonical_root| canonical_path.starts_with(&canonical_root));

    if is_allowed {
        Ok(canonical_path)
    } else {
        Err(format!(
            "Path {} is outside of any loaded project root.",
            canonical_path.display()
        ))
    }
}

#[derive(Deserialize)]
pub struct GetFileContentRequestPayload {
    pub file_path: String,
}

#[derive(Serialize, Deserialize, Debug)]
struct GetFileContentResponseData {
    content: String,
}

pub async fn get_file_content_endpoint(
    request_payload: web::Json<GetFileContentRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let file_path = request_payload.file_path.clone();

    let allowed_roots: Vec<String> = state.pkgs_cache.read().await.keys().cloned().collect();

    let validated_path = match ensure_within_allowed_roots(Path::new(&file_path), &allowed_roots) {
        Ok(path) => path,
        Err(err) => {
            state.out.error_line(&format!("Rejected file read at path {file_path}: {err}"));

            let response = ApiResponse {
                status: Status::Fail,
                data: (),
                meta: None,
            };

            return Ok(HttpResponse::Forbidden().json(response));
        }
    };

    match fs::read_to_string(&validated_path) {
        Ok(content) => {
            let response = ApiResponse {
                status: Status::Ok,
                data: GetFileContentResponseData {
                    content,
                },
                meta: None,
            };
            Ok(HttpResponse::Ok().json(response))
        }
        Err(err) => {
            state.out.error_line(&format!("Error reading file at path {file_path}: {err}"));

            let response = ApiResponse {
                status: Status::Fail,
                data: (),
                meta: None,
            };

            Ok(HttpResponse::BadRequest().json(response))
        }
    }
}

#[derive(Serialize, Deserialize, Debug)]
pub struct SaveFileRequestPayload {
    file_path: String,
    content: String,
}

pub async fn save_file_content_endpoint(
    request_payload: web::Json<SaveFileRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let file_path_str = request_payload.file_path.clone();
    let content = &request_payload.content; // Access the content field.

    let file_path = Path::new(&file_path_str);

    // Attempt to create parent directories if they don't exist.
    if let Some(parent) = file_path.parent() {
        if let Err(e) = fs::create_dir_all(parent) {
            state.out.error_line(&format!(
                "Error creating directories for {}: {}",
                parent.display(),
                e
            ));

            let response = ApiResponse {
                status: Status::Fail,
                data: (),
                meta: None,
            };

            return Ok(HttpResponse::BadRequest().json(response));
        }
    }

    // The target file itself may not exist yet (e.g. a new file being
    // saved for the first time), so canonicalize its now-created parent
    // directory instead and confine the write to a loaded project root.
    let allowed_roots: Vec<String> = state.pkgs_cache.read().await.keys().cloned().collect();
    let parent = file_path.parent().unwrap_or_else(|| Path::new("."));

    let validated_path = match ensure_within_allowed_roots(parent, &allowed_roots) {
        Ok(canonical_parent) => match file_path.file_name() {
            Some(file_name) => canonical_parent.join(file_name),
            None => {
                state.out.error_line(&format!("Invalid file path: {file_path_str}"));

                let response = ApiResponse {
                    status: Status::Fail,
                    data: (),
                    meta: None,
                };

                return Ok(HttpResponse::BadRequest().json(response));
            }
        },
        Err(err) => {
            state.out.error_line(&format!("Rejected file write at path {file_path_str}: {err}"));

            let response = ApiResponse {
                status: Status::Fail,
                data: (),
                meta: None,
            };

            return Ok(HttpResponse::Forbidden().json(response));
        }
    };

    match fs::write(&validated_path, content) {
        Ok(_) => {
            let response = ApiResponse {
                status: Status::Ok,
                data: (),
                meta: None,
            };
            Ok(HttpResponse::Ok().json(response))
        }
        Err(err) => {
            state.out.error_line(&format!(
                "Error writing file at path {}: {}",
                validated_path.display(),
                err
            ));

            let response = ApiResponse {
                status: Status::Fail,
                data: (),
                meta: None,
            };

            Ok(HttpResponse::BadRequest().json(response))
        }
    }
}
