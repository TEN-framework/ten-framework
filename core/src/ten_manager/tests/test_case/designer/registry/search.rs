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
            registry::search::{
                search_packages_endpoint, PkgSearchOptions, SearchPackagesRequestPayload,
                SearchPackagesResponseData,
            },
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
        registry::search::{AtomicFilter, FilterNode, LogicFilter, PkgSearchFilter},
    };

    #[actix_rt::test]
    async fn test_search_packages_from_remote_registry() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };
        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/registry/packages/search",
                web::post().to(search_packages_endpoint),
            ))
            .await;

        let request_payload = SearchPackagesRequestPayload {
            filter: PkgSearchFilter {
                filter: FilterNode::Atomic(AtomicFilter {
                    field: "name".to_string(),
                    operator: "regex".to_string(),
                    value: "^ten_runtime_.*$".to_string(),
                }),
            },
            options: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/registry/packages/search")
            .set_json(&request_payload)
            .to_request();

        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<SearchPackagesResponseData> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(api_response.status, Status::Ok);
        assert!(!api_response.data.packages.is_empty());
    }

    #[actix_rt::test]
    async fn test_search_packages_from_remote_registry_with_logic_filter() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };
        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/registry/packages/search",
                web::post().to(search_packages_endpoint),
            ))
            .await;

        let filter = PkgSearchFilter {
            filter: FilterNode::Logic(LogicFilter::And {
                and: vec![
                    FilterNode::Atomic(AtomicFilter {
                        field: "name".to_string(),
                        operator: "regex".to_string(),
                        value: "^ten_runtime_.*$".to_string(),
                    }),
                    FilterNode::Atomic(AtomicFilter {
                        field: "type".to_string(),
                        operator: "exact".to_string(),
                        value: "system".to_string(),
                    }),
                    FilterNode::Atomic(AtomicFilter {
                        field: "version".to_string(),
                        operator: "exact".to_string(),
                        value: "0.10.20".to_string(),
                    }),
                ],
            }),
        };

        let request_payload = SearchPackagesRequestPayload {
            filter,
            options: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/registry/packages/search")
            .set_json(&request_payload)
            .to_request();

        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<SearchPackagesResponseData> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(api_response.status, Status::Ok);
        assert!(!api_response.data.packages.is_empty());
    }

    #[actix_rt::test]
    async fn test_search_packages_from_remote_registry_with_scope() {
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

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/registry/packages/search",
                web::post().to(search_packages_endpoint),
            ))
            .await;

        let request_payload = SearchPackagesRequestPayload {
            filter: PkgSearchFilter {
                filter: FilterNode::Atomic(AtomicFilter {
                    field: "name".to_string(),
                    operator: "regex".to_string(),
                    value: ".*python.*".to_string(),
                }),
            },
            options: Some(PkgSearchOptions {
                scope: Some("type,name,version,display_name".to_string()),
                page_size: None,
                page: None,
                sort_by: None,
                sort_order: None,
            }),
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/registry/packages/search")
            .set_json(&request_payload)
            .to_request();

        let resp = test::call_service(&app, req).await;

        // Verify the response.
        assert_eq!(resp.status(), StatusCode::OK);

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let api_response: ApiResponse<SearchPackagesResponseData> =
            serde_json::from_str(body_str).unwrap();
        assert_eq!(api_response.status, Status::Ok);
        assert!(!api_response.data.packages.is_empty());
    }
}
