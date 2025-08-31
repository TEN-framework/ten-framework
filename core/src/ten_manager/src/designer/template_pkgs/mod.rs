//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use anyhow::{anyhow, Result};
use semver::Version;
use serde::{Deserialize, Serialize};
use strum_macros::{Display, EnumString};
use ten_rust::pkg_info::pkg_type::PkgType;

use super::DesignerState;
use crate::{
    constants::{TAG_CPP, TAG_GO, TAG_NODEJS, TAG_PYTHON},
    designer::response::{ApiResponse, ErrorResponse, Status},
    registry,
};

#[derive(Deserialize, Serialize, Debug, EnumString, Display, Clone, PartialEq)]
#[strum(serialize_all = "lowercase")]
pub enum TemplateLanguage {
    #[serde(rename = "cpp")]
    Cpp,

    #[serde(rename = "go")]
    Go,

    #[serde(rename = "python")]
    Python,

    #[serde(rename = "nodejs")]
    Nodejs,
}

#[derive(Deserialize, Serialize, Debug)]
pub struct GetTemplateRequestPayload {
    pub pkg_type: PkgType,
    pub language: TemplateLanguage,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct TemplateInfo {
    pub pkg_name: String,
    pub pkg_version: Version,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct GetTemplateResponseData {
    pub templates: Vec<TemplateInfo>,
}

pub async fn get_template_endpoint(
    request_payload: web::Json<GetTemplateRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let GetTemplateRequestPayload {
        pkg_type,
        language,
    } = request_payload.into_inner();

    // Clone the language for later use in error messages.
    let language_clone = language.clone();

    // Determine the tags based on the language.
    let tags = match language {
        TemplateLanguage::Cpp => Some(vec![TAG_CPP.to_string()]),
        TemplateLanguage::Go => Some(vec![TAG_GO.to_string()]),
        TemplateLanguage::Python => Some(vec![TAG_PYTHON.to_string()]),
        TemplateLanguage::Nodejs => Some(vec![TAG_NODEJS.to_string()]),
    };

    // Create configuration and output for calling get_package_list.

    // Call get_package_list with the specified parameters.
    let result = registry::get_package_list(
        state.tman_config.clone(),
        Some(pkg_type),
        None,               // name: None
        None,               // version_req: None
        tags,               // tags based on language
        None,               // scope: None
        None,               // page_size: None
        None,               // page: None
        &state.out.clone(), // output
    )
    .await;

    match result {
        Ok(packages) => {
            // Extract the package names from the PkgRegistryInfo structs.
            let templates: Vec<TemplateInfo> = packages
                .iter()
                .map(|pkg| TemplateInfo {
                    pkg_name: pkg.basic_info.type_and_name.name.clone(),
                    pkg_version: pkg.basic_info.version.clone(),
                })
                .collect();

            // Handle case where no packages were found.
            if templates.is_empty() {
                let error_message = format!(
                    "Unsupported template combination: pkg_type={pkg_type}, \
                     language={language_clone}"
                );

                let error = anyhow!(error_message);
                let error_response =
                    ErrorResponse::from_error(&error, "Unsupported template combination");

                return Ok(HttpResponse::BadRequest().json(error_response));
            }

            let response = ApiResponse {
                status: Status::Ok,
                data: GetTemplateResponseData {
                    templates,
                },
                meta: None,
            };

            Ok(HttpResponse::Ok().json(response))
        }
        Err(err) => {
            let error_message = format!(
                "Failed to fetch templates: pkg_type={pkg_type}, language={language_clone}, \
                 error={err}"
            );

            let error = anyhow!(error_message);
            let error_response = ErrorResponse::from_error(&error, "Failed to fetch templates");

            Ok(HttpResponse::InternalServerError().json(error_response))
        }
    }
}
