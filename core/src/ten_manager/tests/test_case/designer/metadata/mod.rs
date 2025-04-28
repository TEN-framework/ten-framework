//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use actix_web::{http::StatusCode, test, web};
use uuid::Uuid;

use ten_manager::{
    config::{
        metadata::{
            GraphGeometry, GraphUiMetadata, NodeGeometry, TmanMetadata,
        },
        TmanConfig,
    },
    designer::{
        metadata::graph_ui::{
            get::{
                get_graph_ui_endpoint, GetGraphUiRequestPayload,
                GetGraphUiResponseData,
            },
            set::{
                set_graph_ui_endpoint, SetGraphUiRequestPayload,
                SetGraphUiResponseData,
            },
        },
        response::ApiResponse,
        DesignerState,
    },
    output::cli::TmanOutputCli,
};

#[actix_web::test]
async fn test_get_graph_ui_empty() {
    // Generate a random graph_id for testing.
    let graph_id = Uuid::new_v4();

    // Create the request payload.
    let payload = GetGraphUiRequestPayload { graph_id };

    // Create a clean state with empty config.
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with the endpoint.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/graph-ui/get", web::post().to(get_graph_ui_endpoint)),
    )
    .await;

    // Make the request.
    let req = test::TestRequest::post()
        .uri("/graph-ui/get")
        .set_json(&payload)
        .to_request();

    let resp = test::call_service(&app, req).await;

    // Assert status code is OK.
    assert_eq!(resp.status(), StatusCode::OK);

    // Extract and check the response body.
    let body = test::read_body(resp).await;
    let result: ApiResponse<GetGraphUiResponseData> =
        serde_json::from_slice(&body).unwrap();

    // Assert that graph_geometry is None since we haven't set it yet.
    assert!(result.data.graph_geometry.is_none());
}

#[actix_web::test]
async fn test_set_and_get_graph_ui() {
    // Generate a random graph_id for testing.
    let graph_id = Uuid::new_v4();

    // Create test geometry data.
    let node_geometry = NodeGeometry {
        app: Some("test_app".to_string()),
        extension: "test_extension".to_string(),
        x: 100,
        y: 200,
    };

    let graph_geometry = GraphGeometry { nodes_geometry: vec![node_geometry] };

    // Create the set request payload.
    let set_payload = SetGraphUiRequestPayload {
        graph_id,
        graph_geometry: graph_geometry.clone(),
    };

    // Create a clean state with empty config.
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(
            TmanMetadata::default(),
        )),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with both endpoints.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/graph-ui/set", web::post().to(set_graph_ui_endpoint))
            .route("/graph-ui/get", web::post().to(get_graph_ui_endpoint)),
    )
    .await;

    // Make the set request.
    let set_req = test::TestRequest::post()
        .uri("/graph-ui/set")
        .set_json(&set_payload)
        .to_request();

    let set_resp = test::call_service(&app, set_req).await;

    // Assert status code for set is OK.
    assert_eq!(set_resp.status(), StatusCode::OK);

    // Extract and check the set response.
    let set_body = test::read_body(set_resp).await;
    let set_result: ApiResponse<SetGraphUiResponseData> =
        serde_json::from_slice(&set_body).unwrap();

    // Assert that set was successful.
    assert!(set_result.data.success);

    // Now create and make the get request for the same graph_id.
    let get_payload = GetGraphUiRequestPayload { graph_id };

    let get_req = test::TestRequest::post()
        .uri("/graph-ui/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;

    // Assert status code for get is OK.
    assert_eq!(get_resp.status(), StatusCode::OK);

    // Extract and check the get response.
    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetGraphUiResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    // Assert that graph_geometry is now present and matches what we set.
    assert!(get_result.data.graph_geometry.is_some());

    let returned_geometry = get_result.data.graph_geometry.unwrap();
    assert_eq!(returned_geometry.nodes_geometry.len(), 1);
    assert_eq!(returned_geometry.nodes_geometry[0].x, 100);
    assert_eq!(returned_geometry.nodes_geometry[0].y, 200);
    assert_eq!(returned_geometry.nodes_geometry[0].extension, "test_extension");
    assert_eq!(
        returned_geometry.nodes_geometry[0].app,
        Some("test_app".to_string())
    );
}

#[actix_web::test]
async fn test_update_graph_ui() {
    // Generate a random graph_id for testing.
    let graph_id = Uuid::new_v4();

    // Create initial geometry data.
    let initial_node_geometry = NodeGeometry {
        app: Some("initial_app".to_string()),
        extension: "initial_extension".to_string(),
        x: 10,
        y: 20,
    };

    let initial_graph_geometry =
        GraphGeometry { nodes_geometry: vec![initial_node_geometry] };

    // Create updated geometry data.
    let updated_node_geometry = NodeGeometry {
        app: Some("updated_app".to_string()),
        extension: "updated_extension".to_string(),
        x: 30,
        y: 40,
    };

    let updated_graph_geometry =
        GraphGeometry { nodes_geometry: vec![updated_node_geometry] };

    // Create a designer state with pre-filled graph_ui config.
    let mut graph_ui_config = GraphUiMetadata::default();
    graph_ui_config.graphs_geometry.insert(graph_id, initial_graph_geometry);

    let tman_metadata = TmanMetadata { graph_ui: graph_ui_config };

    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(tman_metadata)),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with both endpoints.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/graph-ui/set", web::post().to(set_graph_ui_endpoint))
            .route("/graph-ui/get", web::post().to(get_graph_ui_endpoint)),
    )
    .await;

    // Check initial state with get.
    let get_payload = GetGraphUiRequestPayload { graph_id };
    let get_req = test::TestRequest::post()
        .uri("/graph-ui/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetGraphUiResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    let initial_geo = get_result.data.graph_geometry.unwrap();
    assert_eq!(initial_geo.nodes_geometry[0].x, 10);
    assert_eq!(initial_geo.nodes_geometry[0].y, 20);

    // Now update with new geometry.
    let set_payload = SetGraphUiRequestPayload {
        graph_id,
        graph_geometry: updated_graph_geometry,
    };

    let set_req = test::TestRequest::post()
        .uri("/graph-ui/set")
        .set_json(&set_payload)
        .to_request();

    let set_resp = test::call_service(&app, set_req).await;
    assert_eq!(set_resp.status(), StatusCode::OK);

    // Verify updated geometry.
    let get_req = test::TestRequest::post()
        .uri("/graph-ui/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetGraphUiResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    let updated_geo = get_result.data.graph_geometry.unwrap();
    assert_eq!(updated_geo.nodes_geometry[0].x, 30);
    assert_eq!(updated_geo.nodes_geometry[0].y, 40);
    assert_eq!(updated_geo.nodes_geometry[0].extension, "updated_extension");
    assert_eq!(
        updated_geo.nodes_geometry[0].app,
        Some("updated_app".to_string())
    );
}

#[actix_web::test]
async fn test_get_nonexistent_graph_ui() {
    // Generate two random graph_ids for testing.
    let existing_graph_id = Uuid::new_v4();
    let nonexistent_graph_id = Uuid::new_v4();

    // Create test geometry data.
    let node_geometry = NodeGeometry {
        app: Some("test_app".to_string()),
        extension: "test_extension".to_string(),
        x: 100,
        y: 200,
    };

    let graph_geometry = GraphGeometry { nodes_geometry: vec![node_geometry] };

    // Create a designer state with pre-filled graph_ui config for only one
    // graph.
    let mut graph_ui_config = GraphUiMetadata::default();
    graph_ui_config.graphs_geometry.insert(existing_graph_id, graph_geometry);

    let tman_metadata = TmanMetadata { graph_ui: graph_ui_config };

    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
        tman_metadata: Arc::new(tokio::sync::RwLock::new(tman_metadata)),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
    };
    let state = web::Data::new(Arc::new(designer_state));

    // Create app with the get endpoint.
    let app = test::init_service(
        actix_web::App::new()
            .app_data(state.clone())
            .route("/graph-ui/get", web::post().to(get_graph_ui_endpoint)),
    )
    .await;

    // Make request for the nonexistent graph.
    let get_payload =
        GetGraphUiRequestPayload { graph_id: nonexistent_graph_id };
    let get_req = test::TestRequest::post()
        .uri("/graph-ui/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    assert_eq!(get_resp.status(), StatusCode::OK);

    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetGraphUiResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    // Should return None for nonexistent graph.
    assert!(get_result.data.graph_geometry.is_none());

    // Now check the existing graph.
    let get_payload = GetGraphUiRequestPayload { graph_id: existing_graph_id };
    let get_req = test::TestRequest::post()
        .uri("/graph-ui/get")
        .set_json(&get_payload)
        .to_request();

    let get_resp = test::call_service(&app, get_req).await;
    let get_body = test::read_body(get_resp).await;
    let get_result: ApiResponse<GetGraphUiResponseData> =
        serde_json::from_slice(&get_body).unwrap();

    // Should return Some for existing graph.
    assert!(get_result.data.graph_geometry.is_some());
}
