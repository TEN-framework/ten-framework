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
            extensions::schema::{
                get_extension_schema_endpoint, GetExtensionSchemaRequestPayload,
                GetExtensionSchemaResponseData,
            },
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
        pkg_info::get_all_pkgs::get_all_pkgs_in_app,
    };

    use crate::test_case::common::mock::inject_all_standard_pkgs_for_mock;

    #[actix_web::test]
    async fn test_get_extension_schema_success() {
        // Set up the designer state with initial data.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;
        }

        let designer_state = Arc::new(designer_state);

        // Set up the test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/test_get_extension_schema", web::post().to(get_extension_schema_endpoint)),
        )
        .await;

        // Create request payload for an existing extension
        let req = test::TestRequest::post()
            .uri("/test_get_extension_schema")
            .set_json(&GetExtensionSchemaRequestPayload {
                app_base_dir: TEST_DIR.to_string(),
                addon_name: "extension_addon_1".to_string(),
            })
            .to_request();

        // Send the request and get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is successful.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<GetExtensionSchemaResponseData> =
            serde_json::from_str(body_str).unwrap();

        assert_eq!(api_response.status, Status::Ok);
        // If the extension has schema, it will be included in the response.
        assert!(api_response.data.schema.is_some());

        let expected_schema_str =
            include_str!("../../../../test_data/extension_addon_1_manifest.json");
        let expected_schema_json: serde_json::Value =
            serde_json::from_str(expected_schema_str).unwrap();
        let expected_schema: serde_json::Value =
            serde_json::from_str(expected_schema_json["api"].to_string().as_str()).unwrap();

        assert_eq!(
            serde_json::to_value(api_response.data.schema.unwrap()).unwrap(),
            expected_schema
        );
    }

    #[actix_web::test]
    async fn test_get_extension_schema_extension_not_found() {
        // Set up the designer state with initial data
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            inject_all_standard_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, TEST_DIR).await;
        }

        let designer_state = Arc::new(designer_state);

        // Set up the test service.
        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/test_get_extension_schema_not_found",
                web::post().to(get_extension_schema_endpoint),
            ))
            .await;

        // Create request payload for a non-existent extension.
        let req = test::TestRequest::post()
            .uri("/test_get_extension_schema_not_found")
            .set_json(&GetExtensionSchemaRequestPayload {
                app_base_dir: TEST_DIR.to_string(),
                addon_name: "non_existent_extension".to_string(),
            })
            .to_request();

        // Send the request and get the response
        let resp = test::call_service(&app, req).await;

        // Verify the response is not found
        assert_eq!(resp.status(), StatusCode::NOT_FOUND);
    }

    #[actix_web::test]
    async fn test_get_extension_schema_app_not_found() {
        // Set up the designer state with initial data
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);

        // Set up the test service.
        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/test_get_extension_schema_app_not_found",
                web::post().to(get_extension_schema_endpoint),
            ))
            .await;

        // Create request payload for a non-existent app.
        let req = test::TestRequest::post()
            .uri("/test_get_extension_schema_app_not_found")
            .set_json(&GetExtensionSchemaRequestPayload {
                app_base_dir: "non_existent_app".to_string(),
                addon_name: "test_extension".to_string(),
            })
            .to_request();

        // Send the request and get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is not found.
        assert_eq!(resp.status(), StatusCode::NOT_FOUND);
    }

    #[actix_web::test]
    async fn test_get_extension_schema_with_not_found_interface() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let _ = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &"tests/test_data/extension_interface_reference_not_found".to_string(),
            )
            .await;
        }

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/test_get_extension_schema_with_not_found_interface",
                web::post().to(get_extension_schema_endpoint),
            ))
            .await;

        let request_payload = GetExtensionSchemaRequestPayload {
            app_base_dir: "tests/test_data/extension_interface_reference_not_found".to_string(),
            addon_name: "ext_a".to_string(),
        };

        let req = test::TestRequest::post()
            .uri("/test_get_extension_schema_with_not_found_interface")
            .set_json(&request_payload)
            .to_request();

        let resp = test::call_service(&app, req).await;

        println!("resp: {resp:?}");

        assert_ne!(resp.status(), StatusCode::OK);
        assert_eq!(resp.status(), StatusCode::INTERNAL_SERVER_ERROR);
    }

    #[actix_web::test]
    async fn test_get_extension_schema_with_invalid_interface() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let _ = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &"tests/test_data/extension_interface_reference_invalid".to_string(),
            )
            .await;
        }

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/test_get_extension_schema_with_invalid_interface",
                web::post().to(get_extension_schema_endpoint),
            ))
            .await;

        let request_payload = GetExtensionSchemaRequestPayload {
            app_base_dir: "tests/test_data/extension_interface_reference_invalid".to_string(),
            addon_name: "ext_a".to_string(),
        };

        let req = test::TestRequest::post()
            .uri("/test_get_extension_schema_with_invalid_interface")
            .set_json(&request_payload)
            .to_request();

        let resp = test::call_service(&app, req).await;

        println!("resp: {resp:?}");

        assert_ne!(resp.status(), StatusCode::OK);
        assert_eq!(resp.status(), StatusCode::INTERNAL_SERVER_ERROR);
    }
}
