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
        designer::{
            apps::get::{get_apps_endpoint, GetAppsResponseData},
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
        pkg_info::get_all_pkgs::get_all_pkgs_in_app,
    };

    #[actix_web::test]
    async fn test_get_apps_with_uri() {
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

            let _ = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &"tests/test_data/app_with_uri".to_string(),
            )
            .await;

            assert_eq!(pkgs_cache.get("tests/test_data/app_with_uri").unwrap().len(), 3);
        }

        let designer_state = Arc::new(designer_state);

        // Set up the test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/test_get_apps_with_uri", web::get().to(get_apps_endpoint)),
        )
        .await;

        let req = test::TestRequest::get().uri("/test_get_apps_with_uri").to_request();

        // Send the request and get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<GetAppsResponseData> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(api_response.status, Status::Ok);
        assert_eq!(api_response.data.app_info.len(), 1);
        assert_eq!(api_response.data.app_info[0].base_dir, "tests/test_data/app_with_uri");
        assert_eq!(
            api_response.data.app_info[0].app_uri,
            Some("msgpack://localhost:8000".to_string())
        );
    }

    #[actix_web::test]
    async fn test_get_apps_without_uri() {
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

            let _ = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &"tests/test_data/app_without_uri".to_string(),
            )
            .await;

            assert_eq!(pkgs_cache.get("tests/test_data/app_without_uri").unwrap().len(), 3);
        }

        let designer_state = Arc::new(designer_state);

        // Set up the test service.
        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/test_get_apps_without_uri", web::get().to(get_apps_endpoint)),
        )
        .await;

        let req = test::TestRequest::get().uri("/test_get_apps_without_uri").to_request();

        // Send the request and get the response.
        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<GetAppsResponseData> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(api_response.status, Status::Ok);
        assert_eq!(api_response.data.app_info.len(), 1);
        assert_eq!(api_response.data.app_info[0].base_dir, "tests/test_data/app_without_uri");
        assert_eq!(api_response.data.app_info[0].app_uri, None);
    }
}
