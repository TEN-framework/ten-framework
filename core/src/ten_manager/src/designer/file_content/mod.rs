//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{fs, path::Path, sync::Arc};

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};

use super::{
    response::{ApiResponse, Status},
    DesignerState,
};

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

    match fs::read_to_string(&file_path) {
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

    match fs::write(file_path, content) {
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
                file_path.display(),
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
