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
        designer::{
            apps::load::{load_app_endpoint, LoadAppRequestPayload, LoadAppResponseData},
            response::ApiResponse,
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    #[actix_web::test]
    async fn test_load_app_fail() {
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
                .route("/test_load_app_fail", web::post().to(load_app_endpoint)),
        )
        .await;

        let new_base_dir = LoadAppRequestPayload {
            base_dir: "/not/a/correct/app/folder/path".to_string(),
        };

        let req = test::TestRequest::post()
            .uri("/test_load_app_fail")
            .set_json(&new_base_dir)
            .to_request();
        let resp: Result<ApiResponse<LoadAppResponseData>, std::boxed::Box<dyn std::error::Error>> =
            test::try_call_and_read_body_json(&app, req).await;

        assert!(resp.is_err());
    }
}
