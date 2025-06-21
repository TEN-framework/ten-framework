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
            graphs::get::{
                get_graphs_endpoint, GetGraphsRequestPayload,
                GetGraphsResponseData,
            },
            response::ApiResponse,
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    use crate::test_case::common::mock::inject_all_standard_pkgs_for_mock;

    #[actix_web::test]
    async fn test_get_graphs_success() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(
                TmanConfig::default(),
            )),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(
                TmanStorageInMemory::default(),
            )),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            inject_all_standard_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                TEST_DIR,
            )
            .await;
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs",
                web::post().to(get_graphs_endpoint),
            ),
        )
        .await;

        let request_payload = GetGraphsRequestPayload {};

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let graphs: ApiResponse<Vec<GetGraphsResponseData>> =
            serde_json::from_str(body_str).unwrap();

        let expected_graphs = vec![
            GetGraphsResponseData {
                uuid: "default".to_string(),
                name: Some("default".to_string()),
                auto_start: Some(true),
                base_dir: Some(TEST_DIR.to_string()),
            },
            GetGraphsResponseData {
                uuid: "default_with_app_uri".to_string(),
                name: Some("default_with_app_uri".to_string()),
                auto_start: Some(true),
                base_dir: Some(TEST_DIR.to_string()),
            },
            GetGraphsResponseData {
                uuid: "addon_not_found".to_string(),
                name: Some("addon_not_found".to_string()),
                auto_start: Some(false),
                base_dir: Some(TEST_DIR.to_string()),
            },
        ];

        assert_eq!(graphs.data.len(), expected_graphs.len());

        // Create a map of expected graphs by name for easier lookup.
        let expected_map: HashMap<_, _> =
            expected_graphs.iter().map(|g| (g.name.clone(), g)).collect();

        for actual in graphs.data.iter() {
            let expected =
                expected_map.get(&actual.name).expect("Missing expected graph");
            assert_eq!(actual.name, expected.name);
            assert_eq!(actual.auto_start, expected.auto_start);
            assert_eq!(actual.base_dir, expected.base_dir);
        }

        let json: ApiResponse<Vec<GetGraphsResponseData>> =
            serde_json::from_str(body_str).unwrap();
        let pretty_json = serde_json::to_string_pretty(&json).unwrap();
        println!("Response body: {pretty_json}");
    }

    #[actix_web::test]
    async fn test_get_graphs_no_app_package() {
        let designer_state = Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(
                TmanConfig::default(),
            )),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(
                TmanStorageInMemory::default(),
            )),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        });

        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state)).route(
                "/api/designer/v1/graphs",
                web::post().to(get_graphs_endpoint),
            ),
        )
        .await;

        let request_payload = GetGraphsRequestPayload {};

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());
        println!("Response body: {}", resp.status());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");
    }
}
