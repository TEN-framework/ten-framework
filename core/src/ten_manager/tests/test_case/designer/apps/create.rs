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
    use tempfile::tempdir;
    use ten_manager::{
        constants::DEFAULT_APP_CPP,
        designer::{
            apps::create::{create_app_endpoint, CreateAppRequestPayload},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    #[actix_web::test]
    async fn test_create_app_success() {
        // Create a temporary directory for testing
        let temp_dir = tempdir().unwrap();
        let temp_path = temp_dir.path().to_string_lossy().to_string();

        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig {
                verbose: true,
                ..Default::default()
            })),
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
                .route("/api/designer/v1/apps/create", web::post().to(create_app_endpoint)),
        )
        .await;

        let create_app_request = CreateAppRequestPayload {
            base_dir: temp_path,
            app_name: "test_app".to_string(),
            template_name: DEFAULT_APP_CPP.to_string(),
            template_version: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/apps/create")
            .set_json(&create_app_request)
            .to_request();

        let resp = test::call_service(&app, req).await;
        if !resp.status().is_success() {
            println!("resp: {resp:?}");

            let body = test::read_body(resp).await;
            let body_str = std::str::from_utf8(&body).unwrap();

            println!("body: {body_str:?}");

            panic!("Failed to create app");
        }
    }

    #[actix_web::test]
    async fn test_create_app_invalid_dir() {
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
                .route("/api/designer/v1/apps/create", web::post().to(create_app_endpoint)),
        )
        .await;

        let create_app_request = CreateAppRequestPayload {
            base_dir: "/non/existent/directory".to_string(),
            app_name: "test_app".to_string(),
            template_name: "default".to_string(),
            template_version: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/apps/create")
            .set_json(&create_app_request)
            .to_request();

        let resp = test::call_service(&app, req).await;
        assert!(resp.status().is_client_error());
    }
}
