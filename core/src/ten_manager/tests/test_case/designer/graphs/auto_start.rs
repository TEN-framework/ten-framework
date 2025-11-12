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
            graphs::auto_start::{
                update_graph_auto_start_endpoint, UpdateGraphAutoStartRequestPayload,
                UpdateGraphAutoStartResponseData,
            },
            response::ApiResponse,
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };
    use uuid::Uuid;

    use crate::test_case::common::mock::inject_all_standard_pkgs_for_mock;

    #[actix_web::test]
    async fn test_update_graph_auto_start_success() {
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

        // Find the UUID for the graph with name "default"
        let default_graph_uuid;
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            default_graph_uuid = graphs_cache
                .iter()
                .find_map(|(uuid, graph)| {
                    if graph.name.as_ref().is_some_and(|name| name == "default") {
                        Some(*uuid)
                    } else {
                        None
                    }
                })
                .expect("No graph with name 'default' found");
        }

        let designer_state = Arc::new(designer_state);

        let app =
            test::init_service(App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs/auto-start",
                web::post().to(update_graph_auto_start_endpoint),
            ))
            .await;

        // Test setting auto_start to false
        let request_payload = UpdateGraphAutoStartRequestPayload {
            graph_id: default_graph_uuid,
            auto_start: false,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/auto-start")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        let response: ApiResponse<UpdateGraphAutoStartResponseData> =
            serde_json::from_str(body_str).unwrap();

        assert!(response.data.success);

        // Verify the auto_start field was updated
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let graph_info = graphs_cache.get(&default_graph_uuid).unwrap();
            assert_eq!(graph_info.auto_start, Some(false));
        }

        // Test setting auto_start to true
        let request_payload = UpdateGraphAutoStartRequestPayload {
            graph_id: default_graph_uuid,
            auto_start: true,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/auto-start")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        // Verify the auto_start field was updated to true
        {
            let graphs_cache = designer_state.graphs_cache.read().await;
            let graph_info = graphs_cache.get(&default_graph_uuid).unwrap();
            assert_eq!(graph_info.auto_start, Some(true));
        }
    }

    #[actix_web::test]
    async fn test_update_graph_auto_start_not_found() {
        // Create a designer state with an empty graphs cache.
        let designer_state = Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        });

        // Create a test app with the update_graph_auto_start_endpoint.
        let app = test::init_service(App::new().app_data(web::Data::new(designer_state)).route(
            "/api/designer/v1/graphs/auto-start",
            web::post().to(update_graph_auto_start_endpoint),
        ))
        .await;

        // Use a random UUID that doesn't exist in the cache.
        let nonexistent_graph_id = Uuid::new_v4();

        // Create a request payload with the nonexistent graph ID.
        let request_payload = UpdateGraphAutoStartRequestPayload {
            graph_id: nonexistent_graph_id,
            auto_start: true,
        };

        // Send the request.
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs/auto-start")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Expect a 400 Bad Request response.
        assert_eq!(resp.status(), 400);
    }
}
