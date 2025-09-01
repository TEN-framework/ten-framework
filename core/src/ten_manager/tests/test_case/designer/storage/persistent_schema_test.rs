//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{http::StatusCode, test, web, App};
use serde_json::json;
use ten_manager::{
    designer::{
        response::{ApiResponse, Status},
        storage::{
            in_memory::TmanStorageInMemory,
            persistent::{
                get::{
                    get_persistent_storage_endpoint, GetPersistentRequestPayload,
                    GetPersistentResponseData,
                },
                schema::{
                    set_persistent_storage_schema_endpoint, SetSchemaRequestPayload,
                    SetSchemaResponseData,
                },
                set::{
                    set_persistent_storage_endpoint, SetPersistentRequestPayload,
                    SetPersistentResponseData,
                },
            },
        },
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

fn create_test_designer_state() -> Arc<DesignerState> {
    Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    })
}

#[actix_rt::test]
async fn test_set_schema_success() {
    let designer_state = create_test_designer_state();

    let app = test::init_service(
        App::new()
            .app_data(web::Data::new(designer_state))
            .route("/schema", web::post().to(set_persistent_storage_schema_endpoint)),
    )
    .await;

    let schema = json!({
        "type": "object",
        "properties": {
            "name": {
                "type": "string"
            },
            "age": {
                "type": "integer",
                "minimum": 0
            }
        },
        "required": ["name"]
    });

    let request_payload = SetSchemaRequestPayload {
        schema,
    };

    let req = test::TestRequest::post().uri("/schema").set_json(&request_payload).to_request();

    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<SetSchemaResponseData> = serde_json::from_str(body_str).unwrap();

    assert_eq!(json.status, Status::Ok);
    assert!(json.data.success);
}

#[actix_rt::test]
async fn test_get_without_schema_fails() {
    let designer_state = create_test_designer_state();

    let app = test::init_service(
        App::new()
            .app_data(web::Data::new(designer_state))
            .route("/get", web::post().to(get_persistent_storage_endpoint)),
    )
    .await;

    let request_payload = GetPersistentRequestPayload {
        key: "name".to_string(),
    };

    let req = test::TestRequest::post().uri("/get").set_json(&request_payload).to_request();

    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::BAD_REQUEST);

    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<GetPersistentResponseData> = serde_json::from_str(body_str).unwrap();

    assert_eq!(json.status, Status::Fail);
}

#[actix_rt::test]
async fn test_set_without_schema_fails() {
    let designer_state = create_test_designer_state();

    let app = test::init_service(
        App::new()
            .app_data(web::Data::new(designer_state))
            .route("/set", web::post().to(set_persistent_storage_endpoint)),
    )
    .await;

    let request_payload = SetPersistentRequestPayload {
        key: "name".to_string(),
        value: json!("John"),
    };

    let req = test::TestRequest::post().uri("/set").set_json(&request_payload).to_request();

    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::BAD_REQUEST);

    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<SetPersistentResponseData> = serde_json::from_str(body_str).unwrap();

    assert_eq!(json.status, Status::Fail);
}
