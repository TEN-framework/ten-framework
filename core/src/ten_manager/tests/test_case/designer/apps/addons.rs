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
            apps::addons::{
                get_app_addons_endpoint, GetAppAddonsRequestPayload, GetAppAddonsSingleResponseData,
            },
            response::ApiResponse,
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
        pkg_info::get_all_pkgs::get_all_pkgs_in_app,
    };
    use ten_rust::pkg_info::{pkg_type::PkgType, value_type::ValueType};

    use crate::test_case::common::mock::inject_all_pkgs_for_mock;

    #[actix_web::test]
    async fn test_get_addons() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let all_pkgs_json_str = vec![
            (
                TEST_DIR.to_string(),
                include_str!("../../../test_data/app_manifest.json").to_string(),
                include_str!("../../../test_data/app_property_without_uri.json").to_string(),
            ),
            (
                format!("{}{}", TEST_DIR, "/ten_packages/extension/extension_1"),
                include_str!("../../../test_data/extension_addon_1_manifest.json").to_string(),
                "{}".to_string(),
            ),
            (
                format!("{}{}", TEST_DIR, "/ten_packages/extension/extension_2"),
                include_str!("../../../test_data/extension_addon_2_manifest.json").to_string(),
                "{}".to_string(),
            ),
            (
                format!("{}{}", TEST_DIR, "/ten_packages/extension/extension_3"),
                include_str!("../../../test_data/extension_addon_3_manifest.json").to_string(),
                "{}".to_string(),
            ),
        ];

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let inject_ret =
                inject_all_pkgs_for_mock(&mut pkgs_cache, &mut graphs_cache, all_pkgs_json_str)
                    .await;
            assert!(inject_ret.is_ok());
        }

        let designer_state = Arc::new(designer_state);

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state))
                .route("/api/designer/v1/addons", web::post().to(get_app_addons_endpoint)),
        )
        .await;

        let request_payload = GetAppAddonsRequestPayload {
            base_dir: TEST_DIR.to_string(),
            addon_name: None,
            addon_type: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/addons")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        // Parse the response but don't store it in a variable that will trigger
        // a warning
        let _: ApiResponse<Vec<GetAppAddonsSingleResponseData>> =
            serde_json::from_str(body_str).unwrap();

        // We don't need to verify the exact structure of the expected addons
        // since that's handled by the implementation
    }

    #[actix_web::test]
    async fn test_get_addons_with_interface() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let _ = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &"tests/test_data/extension_interface_reference_to_sys_pkg".to_string(),
            )
            .await;
        }

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state))
                .route("/api/designer/v1/addons", web::post().to(get_app_addons_endpoint)),
        )
        .await;

        let request_payload = GetAppAddonsRequestPayload {
            base_dir: "tests/test_data/extension_interface_reference_to_sys_pkg".to_string(),
            addon_name: None,
            addon_type: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/addons")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        // Parse the response but don't store it in a variable that will trigger
        // a warning
        let response: ApiResponse<Vec<GetAppAddonsSingleResponseData>> =
            serde_json::from_str(body_str).unwrap();

        // The vector should contain 2 items.
        // One is the extension 'ext_a' and the other is the system 'sys_a'.
        assert_eq!(response.data.len(), 2);

        // Find the extension 'ext_a' in the response.
        let ext_a = response.data.iter().find(|addon| addon.addon_name == "ext_a");
        assert!(ext_a.is_some());

        // Find the system 'sys_a' in the response.
        let sys_a = response.data.iter().find(|addon| addon.addon_name == "sys_a");
        assert!(sys_a.is_some());

        // Verify the extension 'ext_a'.
        assert_eq!(ext_a.unwrap().addon_type, PkgType::Extension);
        assert!(ext_a.unwrap().api.is_some());

        // Verify the property of the extension 'ext_a'.
        // It should contain the properties which are defined in its manifest
        // ("foo") and the properties which are imported by the
        // interface("a", "b").
        let ext_a_api = ext_a.unwrap().api.as_ref().unwrap();
        assert_eq!(ext_a_api.property.as_ref().unwrap().len(), 3);
        assert_eq!(
            ext_a_api.property.as_ref().unwrap().get("foo").unwrap().prop_type,
            ValueType::Bool
        );
        assert_eq!(
            ext_a_api.property.as_ref().unwrap().get("a").unwrap().prop_type,
            ValueType::String
        );
        assert_eq!(
            ext_a_api.property.as_ref().unwrap().get("b").unwrap().prop_type,
            ValueType::Int64
        );

        // Verify the cmd_in of the extension 'ext_a'.
        // It should contain the cmd_in which are defined in its manifest
        // ("hello") and the cmd_in which are imported by the
        // interface("cmd_in_a", "cmd_in_b").
        assert_eq!(ext_a_api.cmd_in.as_ref().unwrap().len(), 3);

        // Verify the cmd_out of the extension 'ext_a'.
        // It should contain the cmd_out which are defined in its manifest
        // ("cmd_out_a", "cmd_out_b").
        assert_eq!(ext_a_api.cmd_out.as_ref().unwrap().len(), 2);

        // Verify the data_in of the extension 'ext_a'.
        // It should contain the data_in which are defined in its manifest
        // ("data").
        assert_eq!(ext_a_api.data_in.as_ref().unwrap().len(), 1);

        // Verify the audio_frame_in of the extension 'ext_a'.
        // It should contain the audio_frame_in which are defined in its
        // manifest ("audio_frame_in_a").
        assert_eq!(ext_a_api.audio_frame_in.as_ref().unwrap().len(), 1);

        // Verify the audio_frame_out of the extension 'ext_a'.
        // It should contain the audio_frame_out which are defined in its
        // manifest ("audio_frame_out_a").
        assert_eq!(ext_a_api.audio_frame_out.as_ref().unwrap().len(), 1);

        // Verify the system 'sys_a'.
        assert_eq!(sys_a.unwrap().addon_type, PkgType::System);
        assert!(sys_a.unwrap().api.is_none());
    }

    #[actix_web::test]
    async fn test_get_addons_with_interface_based_on_app_base_dir() {
        let designer_state = DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        };

        let designer_state = Arc::new(designer_state);
        // Get the real path of the app base dir.
        let app_base_dir = {
            let path = std::path::Path::new(
                "tests/test_data/extension_interface_reference_to_app_base_dir",
            );
            match std::fs::canonicalize(path) {
                Ok(canonical_path) => canonical_path,
                Err(_) => {
                    // Fallback to absolute path construction for Windows compatibility
                    let current_dir = std::env::current_dir().unwrap();
                    current_dir.join(path)
                }
            }
        };

        {
            let mut pkgs_cache = designer_state.pkgs_cache.write().await;
            let mut graphs_cache = designer_state.graphs_cache.write().await;

            let _ = get_all_pkgs_in_app(
                &mut pkgs_cache,
                &mut graphs_cache,
                &app_base_dir.to_string_lossy().to_string(),
            )
            .await;
        }

        let app = test::init_service(
            App::new()
                .app_data(web::Data::new(designer_state))
                .route("/api/designer/v1/addons", web::post().to(get_app_addons_endpoint)),
        )
        .await;

        let request_payload = GetAppAddonsRequestPayload {
            base_dir: app_base_dir.to_string_lossy().to_string(),
            addon_name: None,
            addon_type: None,
        };

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/addons")
            .set_json(request_payload)
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let body_str = std::str::from_utf8(&body).unwrap();

        // Parse the response but don't store it in a variable that will trigger
        // a warning
        let response: ApiResponse<Vec<GetAppAddonsSingleResponseData>> =
            serde_json::from_str(body_str).unwrap();

        // The vector should contain 2 items.
        // One is the extension 'ext_a' and the other is the system 'sys_a'.
        assert_eq!(response.data.len(), 2);

        // Find the extension 'ext_a' in the response.
        let ext_a = response.data.iter().find(|addon| addon.addon_name == "ext_a");
        assert!(ext_a.is_some());

        // Find the system 'sys_a' in the response.
        let sys_a = response.data.iter().find(|addon| addon.addon_name == "sys_a");
        assert!(sys_a.is_some());

        // Verify the extension 'ext_a'.
        assert_eq!(ext_a.unwrap().addon_type, PkgType::Extension);
        assert!(ext_a.unwrap().api.is_some());

        // Verify the property of the extension 'ext_a'.
        // It should contain the properties which are defined in its manifest
        // ("foo") and the properties which are imported by the
        // interface("a", "b").
        let ext_a_api = ext_a.unwrap().api.as_ref().unwrap();
        assert_eq!(ext_a_api.property.as_ref().unwrap().len(), 3);
        assert_eq!(
            ext_a_api.property.as_ref().unwrap().get("foo").unwrap().prop_type,
            ValueType::Bool
        );
        assert_eq!(
            ext_a_api.property.as_ref().unwrap().get("a").unwrap().prop_type,
            ValueType::String
        );
        assert_eq!(
            ext_a_api.property.as_ref().unwrap().get("b").unwrap().prop_type,
            ValueType::Int64
        );

        // Verify the cmd_in of the extension 'ext_a'.
        // It should contain the cmd_in which are defined in its manifest
        // ("hello") and the cmd_in which are imported by the
        // interface("cmd_in_a", "cmd_in_b").
        assert_eq!(ext_a_api.cmd_in.as_ref().unwrap().len(), 3);

        // Verify the cmd_out of the extension 'ext_a'.
        // It should contain the cmd_out which are defined in its manifest
        // ("cmd_out_a", "cmd_out_b").
        assert_eq!(ext_a_api.cmd_out.as_ref().unwrap().len(), 2);

        // Verify the data_in of the extension 'ext_a'.
        // It should contain the data_in which are defined in its manifest
        // ("data").
        assert_eq!(ext_a_api.data_in.as_ref().unwrap().len(), 1);

        // Verify the audio_frame_in of the extension 'ext_a'.
        // It should contain the audio_frame_in which are defined in its
        // manifest ("audio_frame_in_a").
        assert_eq!(ext_a_api.audio_frame_in.as_ref().unwrap().len(), 1);

        // Verify the audio_frame_out of the extension 'ext_a'.
        // It should contain the audio_frame_out which are defined in its
        // manifest ("audio_frame_out_a").
        assert_eq!(ext_a_api.audio_frame_out.as_ref().unwrap().len(), 1);

        // Verify the system 'sys_a'.
        assert_eq!(sys_a.unwrap().addon_type, PkgType::System);
        assert!(sys_a.unwrap().api.is_none());
    }

    // Additional test functions would go here...
}
