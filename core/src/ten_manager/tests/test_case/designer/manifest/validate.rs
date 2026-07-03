//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::Arc;

use actix_web::{http::StatusCode, test, web, App};
use ten_manager::{
    designer::{
        manifest::validate::{
            validate_manifest_endpoint, ValidateManifestRequestPayload,
            ValidateManifestResponseData,
        },
        response::{ApiResponse, Status},
        storage::in_memory::TmanStorageInMemory,
        DesignerState,
    },
    home::config::TmanConfig,
    output::cli::TmanOutputCli,
};

#[actix_rt::test]
async fn test_validate_manifest_valid() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Valid manifest JSON.
    let valid_manifest = r#"{
        "type": "extension",
        "name": "test_extension",
        "version": "0.1.0",
        "dependencies": []
    }"#;

    // Create request payload.
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: valid_manifest.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for valid manifest.
    assert_eq!(json.status, Status::Ok);
    assert!(json.data.is_valid);
    assert!(json.data.error_message.is_none());
}

#[actix_rt::test]
async fn test_validate_manifest_with_api() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Valid manifest JSON with API section.
    let valid_manifest = r#"{
        "type": "extension",
        "name": "test_extension",
        "version": "0.1.0",
        "dependencies": [],
        "api": {
          "cmd_out": [
            {
              "name": "test_cmd",
              "property": {
                "properties": {
                  "test_property": {
                    "type": "int8"
                  }
                }
              }
            },
            {
              "name": "has_required",
              "property": {
                "properties": {
                  "foo": {
                    "type": "string"
                  }
                },
                "required": [
                  "foo"
                ]
              }
            },
            {
              "name": "has_required_mismatch",
              "property": {
                "properties": {
                  "foo": {
                    "type": "string"
                  }
                },
                "required": [
                  "foo"
                ]
              }
            }
          ]
        }
      }"#;

    // Create request payload.
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: valid_manifest.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for valid manifest.
    assert_eq!(json.status, Status::Ok);
    assert!(json.data.is_valid);
    assert!(json.data.error_message.is_none());
}

#[actix_rt::test]
async fn test_validate_app_manifest_with_incorrect_api() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Valid manifest JSON with API section.
    let valid_manifest = r#"{
        "type": "app",
        "name": "test_app",
        "version": "0.1.0",
        "dependencies": [],
        "api": {
          "cmd_out": [
            {
              "name": "test_cmd",
              "property": {
                "properties": {
                  "test_property": {
                    "type": "int8"
                  }
                }
              }
            },
            {
              "name": "has_required",
              "property": {
                "properties": {
                  "foo": {
                    "type": "string"
                  }
                },
                "required": [
                  "foo"
                ]
              }
            },
            {
              "name": "has_required_mismatch",
              "property": {
                "properties": {
                  "foo": {
                    "type": "string"
                  }
                },
                "required": [
                  "foo"
                ]
              }
            }
          ]
        }
      }"#;

    // Create request payload.
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: valid_manifest.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for valid manifest.
    assert_eq!(json.status, Status::Ok);
    assert!(!json.data.is_valid);
    assert!(json.data.error_message.is_some());

    println!("error_message: {}", json.data.error_message.unwrap());
}

#[actix_rt::test]
async fn test_validate_app_manifest_with_correct_api() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Valid manifest JSON with API section.
    let valid_manifest = r#"{
        "type": "app",
        "name": "test_app",
        "version": "0.1.0",
        "dependencies": [],
        "api": {
          "property": {
            "properties": {
              "test_property": {
                "type": "int8"
              }
            }
          }
        }
      }"#;

    // Create request payload.
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: valid_manifest.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for valid manifest.
    assert_eq!(json.status, Status::Ok);
    assert!(json.data.is_valid);
    assert!(json.data.error_message.is_none());
}

#[actix_rt::test]
async fn test_validate_manifest_missing_required_fields() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Invalid manifest JSON - missing required fields.
    let invalid_manifest = r#"{
        "type": "extension"
    }"#;

    // Create request payload
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: invalid_manifest.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for invalid manifest.
    assert_eq!(json.status, Status::Ok);
    assert!(!json.data.is_valid);
    assert!(json.data.error_message.is_some());
    let error_message = json.data.error_message.unwrap();
    // Should mention the missing required field.
    assert!(error_message.contains("name"));

    println!("error_message: {error_message}");
}

#[actix_rt::test]
async fn test_validate_manifest_invalid_type() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Invalid manifest JSON - invalid type value.
    let invalid_manifest = r#"{
        "type": "unknown_type",
        "name": "test_extension",
        "version": "0.1.0"
    }"#;

    // Create request payload.
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: invalid_manifest.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for invalid manifest.
    assert_eq!(json.status, Status::Ok);
    assert!(!json.data.is_valid);
    assert!(json.data.error_message.is_some());
    let error_message = json.data.error_message.unwrap();
    // Should mention the invalid type field.
    assert!(error_message.contains("type"));

    println!("error_message: {error_message}");
}

#[actix_rt::test]
async fn test_validate_manifest_invalid_version_format() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Invalid manifest JSON - invalid version format.
    let invalid_manifest = r#"{
        "type": "extension",
        "name": "test_extension",
        "version": "invalid-version"
    }"#;

    // Create request payload.
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: invalid_manifest.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for invalid manifest.
    assert_eq!(json.status, Status::Ok);
    assert!(!json.data.is_valid);
    assert!(json.data.error_message.is_some());
    let error_message = json.data.error_message.unwrap();
    // Should mention the invalid version field.
    assert!(error_message.contains("version"));

    println!("error_message: {error_message}");
}

#[actix_rt::test]
async fn test_validate_manifest_invalid_json_syntax() {
    // Create the application state.
    let designer_state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: Default::default(),
        graphs_cache: Default::default(),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    });

    // Initialize test application with the endpoint.
    let app =
        test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/manifest/validate",
            web::post().to(validate_manifest_endpoint),
        ))
        .await;

    // Invalid JSON syntax.
    let invalid_json = r#"{
        "type": "extension",
        "name": "test_extension",
        "version": "0.1.0",
        "dependencies": [
    }"#; // Missing closing bracket.

    // Create request payload.
    let request_payload = ValidateManifestRequestPayload {
        manifest_json_str: invalid_json.to_string(),
    };

    // Send request to the endpoint.
    let req = test::TestRequest::post()
        .uri("/api/designer/v1/manifest/validate")
        .set_json(&request_payload)
        .to_request();

    // Process the request and get response.
    let resp = test::call_service(&app, req).await;
    assert_eq!(resp.status(), StatusCode::OK);

    // Parse the response body.
    let body = test::read_body(resp).await;
    let body_str = std::str::from_utf8(&body).unwrap();
    let json: ApiResponse<ValidateManifestResponseData> = serde_json::from_str(body_str).unwrap();

    // Assertions for invalid JSON.
    assert_eq!(json.status, Status::Ok);
    assert!(!json.data.is_valid);
    assert!(json.data.error_message.is_some());
    let error_message = json.data.error_message.unwrap();

    // The error message contains information about JSON parsing error
    // It may not literally contain "JSON" but will mention parsing-related
    // errors
    assert!(
        error_message.contains("unexpected character")
            || error_message.contains("expected")
            || error_message.contains("EOF")
            || error_message.contains("syntax")
            || error_message.contains("parse")
    );

    println!("error_message: {error_message}");
}
