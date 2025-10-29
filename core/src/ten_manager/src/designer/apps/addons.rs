//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use ten_rust::pkg_info::{pkg_type::PkgType, PkgInfo};

use crate::designer::{
    common::{get_designer_api_msg_from_pkg, get_designer_api_property_from_pkg},
    graphs::nodes::DesignerApi,
    response::{ApiResponse, ErrorResponse, Status},
    DesignerState,
};

#[derive(Serialize, Deserialize)]
pub struct GetAppAddonsRequestPayload {
    pub base_dir: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub addon_type: Option<PkgType>,

    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub addon_name: Option<String>,
}

#[derive(Serialize, Deserialize, Debug, PartialEq)]
pub struct GetAppAddonsSingleResponseData {
    #[serde(rename = "type")]
    pub addon_type: PkgType,

    #[serde(rename = "name")]
    pub addon_name: String,

    pub url: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub api: Option<DesignerApi>,
}

async fn convert_pkg_info_to_addon(
    pkg_info_with_src: &PkgInfo,
    app_base_dir: Option<&str>,
) -> Result<GetAppAddonsSingleResponseData, ErrorResponse> {
    let manifest_api = pkg_info_with_src
        .manifest
        .get_flattened_api(app_base_dir)
        .await
        .map_err(|e| ErrorResponse::from_error(&e, "Failed to flatten API for extension"));

    if manifest_api.is_err() {
        return Err(manifest_api.err().unwrap());
    }

    let manifest_api = manifest_api.unwrap();

    Ok(GetAppAddonsSingleResponseData {
        addon_type: pkg_info_with_src.manifest.type_and_name.pkg_type,
        addon_name: pkg_info_with_src.manifest.type_and_name.name.clone(),
        url: pkg_info_with_src.url.clone(),
        api: manifest_api.map(|api| DesignerApi {
            property: api
                .property
                .as_ref()
                .map(|prop| get_designer_api_property_from_pkg(prop.clone())),

            cmd_in: api.cmd_in.as_ref().map(|cmd| get_designer_api_msg_from_pkg(cmd.clone())),

            cmd_out: api.cmd_out.as_ref().map(|cmd| get_designer_api_msg_from_pkg(cmd.clone())),

            data_in: api.data_in.as_ref().map(|data| get_designer_api_msg_from_pkg(data.clone())),

            data_out: api.data_out.as_ref().map(|data| get_designer_api_msg_from_pkg(data.clone())),

            audio_frame_in: api
                .audio_frame_in
                .as_ref()
                .map(|data| get_designer_api_msg_from_pkg(data.clone())),

            audio_frame_out: api
                .audio_frame_out
                .as_ref()
                .map(|data| get_designer_api_msg_from_pkg(data.clone())),

            video_frame_in: api
                .video_frame_in
                .as_ref()
                .map(|data| get_designer_api_msg_from_pkg(data.clone())),

            video_frame_out: api
                .video_frame_out
                .as_ref()
                .map(|data| get_designer_api_msg_from_pkg(data.clone())),
        }),
    })
}

pub async fn get_app_addons_endpoint(
    request_payload: web::Json<GetAppAddonsRequestPayload>,
    state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let pkgs_cache = state.pkgs_cache.read().await;

    // Check if base_dir exists in pkgs_cache.
    if request_payload.base_dir.is_empty() || !pkgs_cache.contains_key(&request_payload.base_dir) {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: "Base directory not found or not specified".to_string(),
            error: None,
        };

        return Ok(HttpResponse::NotFound().json(error_response));
    }

    let mut all_addons: Vec<GetAppAddonsSingleResponseData> = Vec::new();

    // Get the PkgsInfoInApp and extract only the requested packages from it.
    if let Some(base_dir_pkg_info) = pkgs_cache.get(&request_payload.base_dir) {
        let addon_type_filter = request_payload.addon_type.as_ref();

        // Only process these packages if no addon_type filter is specified or
        // if it matches "extension"
        if addon_type_filter.is_none() || addon_type_filter == Some(&PkgType::Extension) {
            // Extract extension packages if they exist.
            if let Some(extensions) = &base_dir_pkg_info.extension_pkgs_info {
                for ext in extensions {
                    let addon =
                        convert_pkg_info_to_addon(ext, Some(&request_payload.base_dir)).await;

                    if addon.is_err() {
                        return Ok(HttpResponse::InternalServerError().json(addon.err().unwrap()));
                    }

                    all_addons.push(addon.unwrap());
                }
            }
        }

        // Only process these packages if no addon_type filter is specified or
        // if it matches "protocol"
        if addon_type_filter.is_none() || addon_type_filter == Some(&PkgType::Protocol) {
            // Extract protocol packages if they exist.
            if let Some(protocols) = &base_dir_pkg_info.protocol_pkgs_info {
                for protocol in protocols {
                    let addon =
                        convert_pkg_info_to_addon(protocol, Some(&request_payload.base_dir)).await;

                    if addon.is_err() {
                        return Ok(HttpResponse::InternalServerError().json(addon.err().unwrap()));
                    }

                    all_addons.push(addon.unwrap());
                }
            }
        }

        // Only process these packages if no addon_type filter is specified or
        // if it matches "addon_loader".
        if addon_type_filter.is_none() || addon_type_filter == Some(&PkgType::AddonLoader) {
            // Extract addon loader packages if they exist.
            if let Some(addon_loaders) = &base_dir_pkg_info.addon_loader_pkgs_info {
                for loader in addon_loaders {
                    let addon =
                        convert_pkg_info_to_addon(loader, Some(&request_payload.base_dir)).await;

                    if addon.is_err() {
                        return Ok(HttpResponse::InternalServerError().json(addon.err().unwrap()));
                    }

                    all_addons.push(addon.unwrap());
                }
            }
        }

        // Only process these packages if no addon_type filter is specified or
        // if it matches "system".
        if addon_type_filter.is_none() || addon_type_filter == Some(&PkgType::System) {
            // Extract system packages if they exist.
            if let Some(systems) = &base_dir_pkg_info.system_pkgs_info {
                for system in systems {
                    let addon =
                        convert_pkg_info_to_addon(system, Some(&request_payload.base_dir)).await;

                    if addon.is_err() {
                        return Ok(HttpResponse::InternalServerError().json(addon.err().unwrap()));
                    }

                    all_addons.push(addon.unwrap());
                }
            }
        }
    }

    // Filter by addon_name if provided.
    if let Some(addon_name) = &request_payload.addon_name {
        all_addons.retain(|addon| &addon.addon_name == addon_name);
    }

    // Return success response even if all_addons is empty.
    let response = ApiResponse {
        status: Status::Ok,
        data: all_addons,
        meta: None,
    };

    Ok(HttpResponse::Ok().json(response))
}
