//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod connection;
mod node;

#[cfg(test)]
mod tests {
    use std::{collections::HashMap, sync::Arc};

    use actix_web::{test, web, App};
    use ten_manager::{
        designer::{
            graphs::get::{get_graphs_endpoint, GetGraphsRequestPayload},
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    use crate::test_case::common::mock::inject_all_pkgs_for_mock;

    #[actix_web::test]
    async fn test_get_graphs_with_selector() {
        // Create a designer state with empty caches
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

        // Load the test data from graph_with_selector folder
        let app_manifest_json_str =
            include_str!("../../test_data/graph_with_selector/manifest.json")
                .to_string();
        let app_property_json_str =
            include_str!("../../test_data/graph_with_selector/property.json")
                .to_string();

        // Create test directory name for the app
        let test_app_dir = "/tmp/test_graph_with_selector".to_string();

        let all_pkgs_json = vec![(
            test_app_dir.clone(),
            app_manifest_json_str,
            app_property_json_str,
        )];

        // Inject the test data into caches
        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret = inject_all_pkgs_for_mock(
                &mut pkgs_cache,
                &mut graphs_cache,
                all_pkgs_json,
            )
            .await;
            assert!(inject_ret.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        // Create a test app with the get_graphs_endpoint
        let app = test::init_service(
            App::new().app_data(web::Data::new(designer_state.clone())).route(
                "/api/designer/v1/graphs",
                web::post().to(get_graphs_endpoint),
            ),
        )
        .await;

        // Create a request payload
        let request_payload = GetGraphsRequestPayload {};

        // Make the request
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/graphs")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        // Assert that the response is successful
        assert!(resp.status().is_success());

        // Get the response body
        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();
        println!("Response body: {body_str}");

        // Parse the response
        let response: ApiResponse<
            Vec<ten_manager::designer::graphs::DesignerGraphInfo>,
        > = serde_json::from_str(body_str).unwrap();

        // Verify the response status
        assert_eq!(response.status, Status::Ok);

        // Verify we got exactly one graph (the "default" graph)
        assert_eq!(response.data.len(), 1);

        let graph_info = &response.data[0];

        // Verify the graph name
        assert_eq!(graph_info.name, Some("default".to_string()));

        // Verify the graph has the expected nodes (only extension nodes are
        // converted)
        let nodes = &graph_info.graph.nodes;
        assert_eq!(nodes.len(), 4); // Only the 4 extension nodes, selectors are filtered out

        // Verify all nodes are extension nodes (DesignerGraphNode is a struct
        // for extensions only)
        assert_eq!(nodes.len(), 4);

        // Verify specific extension nodes exist by name
        let extension_names: Vec<&String> =
            nodes.iter().map(|node| &node.name).collect();
        assert!(extension_names.contains(&&"test_extension_1".to_string()));
        assert!(extension_names.contains(&&"test_extension_2".to_string()));
        assert!(extension_names.contains(&&"test_extension_3".to_string()));
        assert!(extension_names.contains(&&"test_extension_4".to_string()));

        // Verify the graph has connections
        let connections = &graph_info.graph.connections;
        assert_eq!(connections.len(), 2);

        // Verify the graph base directory
        assert_eq!(graph_info.base_dir, Some(test_app_dir));

        // Verify auto_start is false (as defined in the test data)
        assert_eq!(graph_info.auto_start, Some(false));
    }
}
