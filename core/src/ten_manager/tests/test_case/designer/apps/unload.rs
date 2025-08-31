//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::{collections::HashMap, sync::Arc};

    use actix_web::{test, web, App};
    use ten_manager::{
        constants::TEST_DIR,
        designer::{
            apps::unload::{unload_app_endpoint, UnloadAppRequestPayload},
            response::{ApiResponse, ErrorResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    use crate::test_case::common::mock::inject_all_pkgs_for_mock;

    #[actix_web::test]
    async fn test_unload_app_success() {
        // Create designer state with an app in pkgs_cache.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Add a simple package to the pkgs_cache.
        let all_pkgs_json_str = vec![(
            TEST_DIR.to_string(),
            include_str!("../../../test_data/app_manifest.json").to_string(),
            include_str!("../../../test_data/app_property_without_uri.json").to_string(),
        )];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json_str)
                    .await;
            assert!(inject_ret.is_ok());

            // Verify that the app is in the pkgs_cache.
            assert!(pkgs_cache.contains_key(TEST_DIR));
        }

        let designer_state = Arc::new(designer_state);

        // Initialize test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/apps/unload", web::post().to(unload_app_endpoint)),
        )
        .await;

        // Create request payload.
        let request_payload = UnloadAppRequestPayload {
            base_dir: TEST_DIR.to_string(),
        };

        // Send request to the test server.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/apps/unload")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        let response: ApiResponse<serde_json::Value> = serde_json::from_str(body_str).unwrap();

        assert_eq!(response.status, Status::Ok);
        assert_eq!(response.data["success"], true);

        let pkgs_cache = designer_state.pkgs_cache.read().await;
        assert!(!pkgs_cache.contains_key(TEST_DIR));
    }

    #[actix_web::test]
    async fn test_unload_app_invalid_base_dir() {
        // Create designer state without any apps in pkgs_cache.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);

        // Initialize test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/apps/unload", web::post().to(unload_app_endpoint)),
        )
        .await;

        // Create request payload with a non-existent base_dir.
        let request_payload = UnloadAppRequestPayload {
            base_dir: "non_existent_dir".to_string(),
        };

        // Send request to the test server.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/apps/unload")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Verify the response is an error.
        assert_eq!(resp.status(), actix_web::http::StatusCode::BAD_REQUEST);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        let error_response: ErrorResponse = serde_json::from_str(body_str).unwrap();

        assert_eq!(error_response.status, Status::Fail);
        assert!(!error_response.message.is_empty());
    }

    #[actix_web::test]
    async fn test_unload_app_not_loaded() {
        // Create designer state without any apps in pkgs_cache.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);

        // Initialize test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/apps/unload", web::post().to(unload_app_endpoint)),
        )
        .await;

        // Create request payload with a valid but not loaded base_dir.
        let request_payload = UnloadAppRequestPayload {
            base_dir: TEST_DIR.to_string(),
        };

        // Send request to the test server.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/apps/unload")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Verify the response is an error.
        assert_eq!(resp.status(), actix_web::http::StatusCode::BAD_REQUEST);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        let error_response: ErrorResponse = serde_json::from_str(body_str).unwrap();

        assert_eq!(error_response.status, Status::Fail);
        assert!(error_response.message.contains("not loaded"));
    }
}
