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
    use serde::{Deserialize, Serialize};
    use ten_manager::{
        designer::{
            response::ApiResponse, storage::in_memory::TmanStorageInMemory,
            version::get_version_endpoint, DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
        version::VERSION,
    };

    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct GetVersionResponseData {
        version: String,
    }

    #[actix_web::test]
    async fn test_get_version() {
        // Initialize the DesignerState.
        let state = web::Data::new(Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        }));

        // Create the App with the routes configured.
        let app = test::init_service(
            App::new()
                .app_data(state.clone())
                .route("/api/designer/v1/version", web::get().to(get_version_endpoint)),
        )
        .await;

        // Send a request to the version endpoint.
        let req = test::TestRequest::get().uri("/api/designer/v1/version").to_request();
        let resp = test::call_service(&app, req).await;

        // Check that the response status is 200 OK.
        assert_eq!(resp.status(), StatusCode::OK);

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let version: ApiResponse<GetVersionResponseData> = serde_json::from_str(body_str).unwrap();

        // Create the expected Version struct
        let expected_version = GetVersionResponseData {
            version: VERSION.to_string(),
        };

        // Compare the actual Version struct with the expected one
        assert_eq!(version.data, expected_version);

        let json: ApiResponse<GetVersionResponseData> = serde_json::from_str(body_str).unwrap();
        let pretty_json = serde_json::to_string_pretty(&json).unwrap();
        println!("Response body: {pretty_json}");
    }
}
