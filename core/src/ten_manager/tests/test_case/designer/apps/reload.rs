//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::{collections::HashMap, sync::Arc};

    use actix_web::{http::StatusCode, test, web, App};
    use ten_manager::{
        constants::TEST_DIR,
        designer::{
            apps::reload::{reload_app_endpoint, ReloadPkgsRequestPayload},
            response::{ErrorResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use ten_rust::base_dir_pkg_info::PkgsInfoInApp;

    use crate::test_case::common::mock::inject_all_pkgs_for_mock;

    /// Test error case when the specified base_dir doesn't exist in pkgs_cache.
    #[actix_web::test]
    async fn test_reload_app_error_base_dir_not_found() {
        // Set up the designer state.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // Inject initial package data.
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
        }

        let designer_state = Arc::new(designer_state);

        // Set up the test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/apps/reload", web::post().to(reload_app_endpoint)),
        )
        .await;

        // Create request with non-existent base_dir.
        let request_payload = ReloadPkgsRequestPayload {
            base_dir: Some("/non/existent/base/dir".to_string()),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/apps/reload")
            .set_json(request_payload)
            .to_request();

        // Send the request and get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is an error.
        assert_eq!(resp.status(), StatusCode::BAD_REQUEST);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let error_response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(error_response.status, Status::Fail);
        assert_eq!(
            error_response.message,
            "App with base_dir '/non/existent/base/dir' is not loaded"
        );
    }

    /// Test the scenario where get_all_pkgs fails during reload.
    #[actix_web::test]
    async fn test_reload_app_error_get_all_pkgs_fails() {
        // For this test, we'll need to mock get_all_pkgs to fail. This is a
        // bit complicated in Rust without mocking frameworks. As a basic
        // approach, we'll remove the base_dir from pkgs_cache and then use a
        // non-existent path in base_dir that will likely cause get_all_pkgs to
        // fail.

        // Set up the designer state.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig {
                verbose: true,
                ..TmanConfig::default()
            })),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        // We'll use a special path that should cause get_all_pkgs to fail.
        // This is a bit of a hack but should work for testing purposes.
        let invalid_path = "/definitely/invalid/path/that/doesnt/exist";

        // Create an empty PkgsInfoInApp.
        let empty_pkg_info = PkgsInfoInApp::default();

        // Inject an entry with the invalid path.
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            pkgs_cache.insert(invalid_path.to_string(), empty_pkg_info);
        }

        let designer_state = Arc::new(designer_state);

        // Set up the test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/apps/reload", web::post().to(reload_app_endpoint)),
        )
        .await;

        // Create request with the invalid base_dir.
        let request_payload = ReloadPkgsRequestPayload {
            base_dir: Some(invalid_path.to_string()),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/apps/reload")
            .set_json(request_payload)
            .to_request();

        // Send the request and get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is an error - this should be 500 Internal Server
        // Error.
        assert!(resp.status().is_server_error());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let error_response: ErrorResponse = serde_json::from_str(body_str).unwrap();
        assert_eq!(error_response.status, Status::Fail);
        // The exact error message will depend on the implementation of
        // get_all_pkgs.
        assert!(error_response.message.contains("Failed to reload packages:"));
    }
}
