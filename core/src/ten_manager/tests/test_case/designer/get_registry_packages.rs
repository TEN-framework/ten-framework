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
            registry::packages::{get_packages_endpoint, GetPackagesResponseData},
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use ten_rust::pkg_info::pkg_type::PkgType;

    use crate::test_case::common::builtin_server::start_test_server;

    #[actix_rt::test]
    async fn test_get_packages_success() {
        // Start the http server and get its address.
        let server_addr =
            start_test_server("/api/designer/v1/packages", || web::get().to(get_packages_endpoint))
                .await;
        println!("Server started at: {server_addr}");

        // Create query parameters
        let pkg_type = PkgType::Extension;
        let name = "ext_a";
        let version_req = "1.0.0";

        // Create a reqwest client to send the request.
        let client = reqwest::Client::builder()
            .no_proxy() // Disable using proxy from environment variables.
            .build()
            .expect("Failed to build client");

        // Construct the full URL using the server address with query
        // parameters.
        let url = format!(
            "http://{server_addr}/api/designer/v1/packages?pkg_type={pkg_type}&name={name}&version_req={version_req}"
        );
        println!("Sending request to URL: {url}");

        // Send the GET request with query parameters.
        let response = client.get(&url).send().await.expect("Failed to send request");

        assert_eq!(response.status(), 200);
        let body = response.text().await.expect("Failed to read response");
        println!("Response body: {body}");
    }

    #[actix_rt::test]
    async fn test_get_packages_from_remote_registry() {
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
                .route("/api/designer/v1/registry/packages", web::get().to(get_packages_endpoint)),
        )
        .await;

        // Create query parameters
        let pkg_type = PkgType::Extension;
        let name = "pil_demo_python";
        let version_req = "0.10.18";

        let req = test::TestRequest::get()
            .uri(&format!(
                "/api/designer/v1/registry/packages?pkg_type={pkg_type}&name={name}&\
                 version_req={version_req}"
            ))
            .to_request();

        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<GetPackagesResponseData> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(api_response.status, Status::Ok);
        assert_eq!(api_response.data.packages.len(), 1);
        assert_eq!(api_response.data.packages[0].display_name.as_ref().unwrap().locales.len(), 5);
    }

    #[actix_rt::test]
    async fn test_get_packages_from_remote_registry_with_scope() {
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
        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state.clone()))
                .route("/api/designer/v1/registry/packages", web::get().to(get_packages_endpoint)),
        )
        .await;

        // Create query parameters
        let pkg_type = PkgType::Extension;
        let name = "pil_demo_python";
        let version_req = "0.10.18";
        let scope = "type,name,version,downloadUrl,dependencies,hash";

        let req = test::TestRequest::get()
            .uri(&format!(
                "/api/designer/v1/registry/packages?pkg_type={pkg_type}&name={name}&\
                 version_req={version_req}&scope={scope}"
            ))
            .to_request();

        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<GetPackagesResponseData> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(api_response.status, Status::Ok);
        assert_eq!(api_response.data.packages.len(), 1);

        // The display_name should be None because 'display_name' is not
        // included in the scope.
        assert!(api_response.data.packages[0].display_name.is_none());
    }
}
