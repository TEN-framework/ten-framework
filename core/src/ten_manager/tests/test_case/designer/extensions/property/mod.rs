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
            extensions::property::{
                get_extension_property_endpoint, GetExtensionPropertyRequestPayload,
                GetExtensionPropertyResponseData,
            },
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    use crate::test_case::common::mock::{
        inject_all_pkgs_for_mock, inject_all_standard_pkgs_for_mock,
    };

    #[actix_web::test]
    async fn test_get_extension_property_success() {
        // Set up the designer state with initial data.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let all_pkgs_json_str = vec![
            (
                TEST_DIR.to_string(),
                include_str!("../../../../test_data/app_manifest.json").to_string(),
                include_str!("../../../../test_data/app_property.json").to_string(),
            ),
            (
                format!("{}{}", TEST_DIR, "/ten_packages/extension/extension_addon_1"),
                include_str!("../../../../test_data/extension_addon_1_manifest.json").to_string(),
                include_str!("../../../../test_data/extension_addon_1_property.json").to_string(),
            ),
        ];

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
        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/extensions/property",
            web::post().to(get_extension_property_endpoint),
        ))
        .await;

        // Create the request payload.
        let request_payload = GetExtensionPropertyRequestPayload {
            app_base_dir: TEST_DIR.to_string(),
            addon_name: "extension_addon_1".to_string(),
        };

        // Make the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/extensions/property")
            .set_json(request_payload)
            .to_request();

        // Get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is successful.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<GetExtensionPropertyResponseData> =
            serde_json::from_str(body_str).unwrap();

        // Verify the response status is OK.
        assert_eq!(api_response.status, Status::Ok);

        // Verify the property is returned correctly.
        assert!(api_response.data.property.is_some());
        println!("api_response.data.property: {:?}", api_response.data.property);

        let property = api_response.data.property.unwrap();
        assert_eq!(property.get("key1").unwrap().as_str().unwrap(), "value1");
        assert_eq!(property.get("key2").unwrap().as_i64().unwrap(), 42);
        assert!(property.get("key3").unwrap().is_object());
    }

    #[actix_web::test]
    async fn test_get_extension_property_app_not_found() {
        // Set up the designer state with empty cache.
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);

        // Set up the test service
        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/extensions/property",
            web::post().to(get_extension_property_endpoint),
        ))
        .await;

        // Create the request payload with non-existent app.
        let request_payload = GetExtensionPropertyRequestPayload {
            app_base_dir: "non_existent_app".to_string(),
            addon_name: "test_extension".to_string(),
        };

        // Make the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/extensions/property")
            .set_json(request_payload)
            .to_request();

        // Get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is not found.
        assert_eq!(resp.status(), StatusCode::NOT_FOUND);
    }

    #[actix_web::test]
    async fn test_get_extension_property_extension_not_found() {
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
        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/extensions/property",
            web::post().to(get_extension_property_endpoint),
        ))
        .await;

        // Create the request payload with non-existent extension.
        let request_payload = GetExtensionPropertyRequestPayload {
            app_base_dir: TEST_DIR.to_string(),
            addon_name: "non_existent_extension".to_string(),
        };

        // Make the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/extensions/property")
            .set_json(request_payload)
            .to_request();

        // Get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is not found.
        assert_eq!(resp.status(), StatusCode::NOT_FOUND);
    }

    #[actix_web::test]
    async fn test_get_extension_property_no_property() {
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
        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/extensions/property",
            web::post().to(get_extension_property_endpoint),
        ))
        .await;

        // Create the request payload.
        let request_payload = GetExtensionPropertyRequestPayload {
            app_base_dir: TEST_DIR.to_string(),
            addon_name: "extension_addon_1".to_string(),
        };

        // Make the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/extensions/property")
            .set_json(request_payload)
            .to_request();

        // Get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response is successful (even with empty property).
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<GetExtensionPropertyResponseData> =
            serde_json::from_str(body_str).unwrap();

        // Verify the response status is OK.
        assert_eq!(api_response.status, Status::Ok);

        // Verify property is None since there's no property data.
        assert!(api_response.data.property.is_none());
    }
}
