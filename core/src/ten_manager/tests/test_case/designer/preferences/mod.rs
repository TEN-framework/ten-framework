//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;
use std::sync::{Arc, RwLock};

use actix_web::{test, web, App};
use serde_json::{self, json};

use ten_manager::config::metadata::TmanMetadata;
use ten_manager::{
    config::TmanConfig,
    designer::{
        locale::Locale,
        preferences::{
            get::get_preferences_endpoint,
            get_schema::get_preferences_schema_endpoint,
            update::{
                update_preferences_endpoint, UpdatePreferencesRequestPayload,
            },
            update_field::{
                update_preferences_field_endpoint,
                UpdatePreferencesFieldRequestPayload,
            },
            Designer,
        },
        DesignerState,
    },
    output::cli::TmanOutputCli,
};

// Tests from get.rs
#[actix_web::test]
async fn test_get_preferences_success() {
    // Create test state.
    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    });

    // Create test app.
    let app = test::init_service(
        App::new().app_data(web::Data::new(state)).service(
            web::scope("/api/designer/v1")
                .route("/preferences", web::get().to(get_preferences_endpoint)),
        ),
    )
    .await;

    // Create test request.
    let req = test::TestRequest::get()
        .uri("/api/designer/v1/preferences")
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 200 OK.
    assert!(resp.status().is_success());

    // Parse response body.
    let body = test::read_body(resp).await;
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();

    // Assert response structure.
    assert_eq!(json["status"], "ok");
    assert!(json["data"].is_object());
    assert!(json["data"]["preferences"].is_object());

    // Verify preferences contains the expected properties from the default
    // TmanConfig.
    let preferences = &json["data"]["preferences"];
    assert!(preferences["logviewer_line_size"].is_number());
    assert!(preferences["locale"].is_string());

    // Check default values.
    assert_eq!(preferences["locale"], "en-US");
}

#[actix_web::test]
async fn test_get_preferences_invalid_path() {
    // Create test state.
    let state = Arc::new(RwLock::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    }));

    // Create test app.
    let app = test::init_service(
        App::new().app_data(web::Data::new(state)).service(
            web::scope("/api/designer/v1")
                .route("/preferences", web::get().to(get_preferences_endpoint)),
        ),
    )
    .await;

    // Create test request with invalid path.
    let req =
        test::TestRequest::get().uri("/api/designer/v1/invalid").to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 404 Not Found.
    assert_eq!(resp.status(), 404);
}

// Tests from get_schema.rs
#[actix_web::test]
async fn test_get_preferences_schema_success() {
    // Create test app.
    let app = test::init_service(App::new().service(
        web::scope("/api/designer/v1").route(
            "/preferences/schema",
            web::get().to(get_preferences_schema_endpoint),
        ),
    ))
    .await;

    // Create test request.
    let req = test::TestRequest::get()
        .uri("/api/designer/v1/preferences/schema")
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 200 OK.
    assert!(resp.status().is_success());

    // Parse response body.
    let body = test::read_body(resp).await;
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();

    // Assert response structure.
    assert_eq!(json["status"], "ok");
    assert!(json["data"].is_object());
    assert!(json["data"]["schema"].is_object());

    // Verify schema contains the expected properties.
    let schema = &json["data"]["schema"];
    assert_eq!(schema["title"], "Designer");
    assert_eq!(schema["type"], "object");
    assert!(schema["properties"].is_object());
    assert!(schema["properties"]["logviewer_line_size"].is_object());
}

#[actix_web::test]
async fn test_get_preferences_schema_invalid_path() {
    // Create test app.
    let app = test::init_service(App::new().service(
        web::scope("/api/designer/v1").route(
            "/preferences/schema",
            web::get().to(get_preferences_schema_endpoint),
        ),
    ))
    .await;

    // Create test request with invalid path.
    let req = test::TestRequest::get()
        .uri("/api/designer/v1/preferences/invalid")
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 404 Not Found.
    assert_eq!(resp.status(), 404);
}

// Tests from update.rs
#[actix_web::test]
async fn test_update_preferences_success() {
    // Create test state with mock config file path to avoid writing to real
    // file.
    let config = TmanConfig { config_file: None, ..TmanConfig::default() };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    });

    // Create test app.
    let app = test::init_service(
        App::new().app_data(web::Data::new(state.clone())).service(
            web::scope("/api/designer/v1").route(
                "/preferences",
                web::put().to(update_preferences_endpoint),
            ),
        ),
    )
    .await;

    // Create valid preferences payload.
    let payload = UpdatePreferencesRequestPayload {
        preferences: Designer {
            logviewer_line_size: 2000, // Valid value according to schema.
            locale: Locale::EnUs,
        },
    };

    // Create test request.
    let req = test::TestRequest::put()
        .uri("/api/designer/v1/preferences")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 200 OK.
    assert!(resp.status().is_success());

    // Parse response body.
    let body = test::read_body(resp).await;
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();

    // Assert response structure.
    assert_eq!(json["status"], "ok");

    // Verify config was updated (though we're not actually writing to file
    // in test).
    assert_eq!(
        state.tman_config.read().await.designer.logviewer_line_size,
        2000
    );
    assert!(matches!(
        state.tman_config.read().await.designer.locale,
        Locale::EnUs
    ));
}

#[actix_web::test]
async fn test_update_preferences_invalid_schema() {
    // Create test state.
    let config = TmanConfig { config_file: None, ..TmanConfig::default() };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    });

    // Create test app.
    let app =
        test::init_service(App::new().app_data(web::Data::new(state)).service(
            web::scope("/api/designer/v1").route(
                "/preferences",
                web::put().to(update_preferences_endpoint),
            ),
        ))
        .await;

    // Create invalid preferences payload (violates schema minimum
    // constraint).
    let payload = UpdatePreferencesRequestPayload {
        preferences: Designer {
            logviewer_line_size: 50, // Invalid value - minimum is 100.
            locale: Locale::default(),
        },
    };

    // Create test request.
    let req = test::TestRequest::put()
        .uri("/api/designer/v1/preferences")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 400 Bad Request due to schema violation.
    assert_eq!(resp.status(), 400);
}

// Tests from update_field.rs
#[actix_web::test]
async fn test_update_preferences_field_success() {
    // Create test state with mock config file path to avoid writing to real
    // file.
    let config = TmanConfig { config_file: None, ..TmanConfig::default() };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    });

    // Create test app.
    let app = test::init_service(
        App::new().app_data(web::Data::new(state.clone())).service(
            web::scope("/api/designer/v1").route(
                "/preferences/field",
                web::patch().to(update_preferences_field_endpoint),
            ),
        ),
    )
    .await;

    // Create valid field update payload.
    let payload = UpdatePreferencesFieldRequestPayload {
        field: "logviewer_line_size".to_string(),
        value: json!(2000), // Valid value according to schema.
    };

    // Create test request.
    let req = test::TestRequest::patch()
        .uri("/api/designer/v1/preferences/field")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 200 OK.
    assert!(resp.status().is_success());

    // Parse response body.
    let body = test::read_body(resp).await;
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();

    // Assert response structure.
    assert_eq!(json["status"], "ok");

    // Verify config was updated (though we're not actually writing to file
    // in test).
    assert_eq!(
        state.tman_config.read().await.designer.logviewer_line_size,
        2000
    );
}

#[actix_web::test]
async fn test_update_preferences_field_invalid_value() {
    // Create test state.
    let config = TmanConfig { config_file: None, ..TmanConfig::default() };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    });

    // Create test app.
    let app =
        test::init_service(App::new().app_data(web::Data::new(state)).service(
            web::scope("/api/designer/v1").route(
                "/preferences/field",
                web::patch().to(update_preferences_field_endpoint),
            ),
        ))
        .await;

    // Create invalid field update payload (violates schema minimum
    // constraint).
    let payload = UpdatePreferencesFieldRequestPayload {
        field: "logviewer_line_size".to_string(),
        value: json!(50), // Invalid value - minimum is 100.
    };

    // Create test request.
    let req = test::TestRequest::patch()
        .uri("/api/designer/v1/preferences/field")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 400 Bad Request due to schema violation.
    assert_eq!(resp.status(), 400);
}

#[actix_web::test]
async fn test_update_preferences_field_invalid_field() {
    // Create test state.
    let config = TmanConfig { config_file: None, ..TmanConfig::default() };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    });

    // Create test app.
    let app =
        test::init_service(App::new().app_data(web::Data::new(state)).service(
            web::scope("/api/designer/v1").route(
                "/preferences/field",
                web::patch().to(update_preferences_field_endpoint),
            ),
        ))
        .await;

    // Create payload with a field that doesn't exist in the schema
    // definition.
    let payload = UpdatePreferencesFieldRequestPayload {
        // Field doesn't exist in schema.
        field: "non_existent_field".to_string(),
        value: json!(2000),
    };

    // Create test request.
    let req = test::TestRequest::patch()
        .uri("/api/designer/v1/preferences/field")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // This test verifies that our schema validation rejects additional
    // properties. The JSON schema for designer frontend preferences (in
    // schema/data/designer.schema.json) now has additionalProperties:
    // false, so adding a non-existent field should be rejected.
    assert_eq!(resp.status(), 400);
}

#[actix_web::test]
async fn test_update_preferences_field_locale() {
    // Create test state with mock config file path to avoid writing to real
    // file.
    let config = TmanConfig { config_file: None, ..TmanConfig::default() };

    let state = Arc::new(DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(config)),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    });

    // Create test app.
    let app = test::init_service(
        App::new().app_data(web::Data::new(state.clone())).service(
            web::scope("/api/designer/v1").route(
                "/preferences/field",
                web::patch().to(update_preferences_field_endpoint),
            ),
        ),
    )
    .await;

    // Create valid field update payload.
    let payload = UpdatePreferencesFieldRequestPayload {
        field: "locale".to_string(),
        value: json!("zh-CN"), // Valid value according to schema.
    };

    // Create test request.
    let req = test::TestRequest::patch()
        .uri("/api/designer/v1/preferences/field")
        .set_json(&payload)
        .to_request();
    let resp = test::call_service(&app, req).await;

    // Assert response status is 200 OK.
    assert!(resp.status().is_success());

    // Parse response body.
    let body = test::read_body(resp).await;
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();

    // Assert response structure.
    assert_eq!(json["status"], "ok");

    // Verify config was updated (though we're not actually writing to file
    // in test).
    assert!(matches!(
        state.tman_config.read().await.designer.locale,
        Locale::ZhCn
    ));
}
