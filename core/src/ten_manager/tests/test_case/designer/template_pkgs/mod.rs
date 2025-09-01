//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{http::StatusCode, test, web, App};
use ten_manager::{
    designer::{
        response::{ApiResponse, Status},
        storage::in_memory::TmanStorageInMemory,
        template_pkgs::{
            get_template_endpoint, GetTemplateRequestPayload, GetTemplateResponseData,
            TemplateLanguage,
        },
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};
use ten_rust::pkg_info::pkg_type::PkgType;

#[actix_web::test]
async fn test_get_template_app_typescript() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let designer_state = Arc::new(designer_state);

    let app = test::init_service(
        App::new()
            .app_data(web::Data::new(designer_state.clone()))
            .route("/api/designer/v1/template-pkgs", web::post().to(get_template_endpoint)),
    )
    .await;

    let request_payload = GetTemplateRequestPayload {
        pkg_type: PkgType::App,
        language: TemplateLanguage::Nodejs,
    };

    let req = test::TestRequest::post()
        .uri("/api/designer/v1/template-pkgs")
        .set_json(&request_payload)
        .to_request();

    let resp: ApiResponse<GetTemplateResponseData> = test::call_and_read_body_json(&app, req).await;

    assert_eq!(resp.status, Status::Ok);
    println!("{:?}", resp.data.templates);
}

#[actix_web::test]
async fn test_get_template_extension_cpp() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let designer_state = Arc::new(designer_state);

    let app = test::init_service(
        App::new()
            .app_data(web::Data::new(designer_state.clone()))
            .route("/api/designer/v1/template-pkgs", web::post().to(get_template_endpoint)),
    )
    .await;

    let request_payload = GetTemplateRequestPayload {
        pkg_type: PkgType::Extension,
        language: TemplateLanguage::Cpp,
    };

    let req = test::TestRequest::post()
        .uri("/api/designer/v1/template-pkgs")
        .set_json(&request_payload)
        .to_request();

    let resp: ApiResponse<GetTemplateResponseData> = test::call_and_read_body_json(&app, req).await;

    assert_eq!(resp.status, Status::Ok);
    println!("{:?}", resp.data.templates);
}

#[actix_web::test]
async fn test_get_template_unsupported() {
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };
    let designer_state = Arc::new(designer_state);

    let app = test::init_service(
        App::new()
            .app_data(web::Data::new(designer_state.clone()))
            .route("/api/designer/v1/template-pkgs", web::post().to(get_template_endpoint)),
    )
    .await;

    // Create a request with an unsupported PkgType and Language
    // combination.
    let request_payload = GetTemplateRequestPayload {
        pkg_type: PkgType::Invalid,
        language: TemplateLanguage::Nodejs,
    };

    let req = test::TestRequest::post()
        .uri("/api/designer/v1/template-pkgs")
        .set_json(&request_payload)
        .to_request();

    let resp = test::call_service(&app, req).await;

    // Expect a 400 Bad Request response.
    assert_eq!(resp.status(), StatusCode::BAD_REQUEST);
}
