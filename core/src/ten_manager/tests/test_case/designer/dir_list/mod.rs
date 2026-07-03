//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::{
        collections::HashMap,
        fs::{self, File},
        io::Write,
        sync::Arc,
    };

    use actix_web::{test, web, App};
    use serde::{Deserialize, Serialize};
    use tempfile::tempdir;
    use ten_manager::{
        designer::{
            dir_list::list_dir_endpoint,
            response::{ApiResponse, Status},
            storage::in_memory::TmanStorageInMemory,
            DesignerState,
        },
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
    };

    #[derive(Serialize, Deserialize, Debug, Clone)]
    struct FsEntry {
        name: String,
        path: String,
        is_dir: bool,
    }

    #[derive(Serialize, Deserialize)]
    struct ListDirRequestPayload {
        path: String,
    }

    #[derive(Serialize, Deserialize)]
    struct DirListResponseData {
        entries: Vec<FsEntry>,
    }

    #[actix_web::test]
    async fn test_list_dir_with_file_path() {
        // Create a temporary directory.
        let dir = tempdir().unwrap();
        let file_path = dir.path().join("test_file.txt");
        let mut file = File::create(&file_path).unwrap();
        writeln!(file, "Hello, world!").unwrap();

        // Initialize DesignerState.
        let state = web::Data::new(Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        }));

        // Configure the `list_dir` route.
        let app = test::init_service(
            App::new()
                .app_data(state.clone())
                .route("/api/designer/v1/dir-list", web::post().to(list_dir_endpoint)),
        )
        .await;

        // Construct the request.
        let req_path = file_path.to_string_lossy().to_string();

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/dir-list")
            .set_json(ListDirRequestPayload {
                path: req_path,
            })
            .to_request();

        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let response: ApiResponse<DirListResponseData> = serde_json::from_slice(&body).unwrap();

        assert_eq!(response.status, Status::Ok);
        assert_eq!(response.data.entries.len(), 1);
        let entry = &response.data.entries[0];
        assert_eq!(entry.name, "test_file.txt");
        assert!(!entry.is_dir);
    }

    #[actix_web::test]
    async fn test_list_dir_with_directory_path() {
        // Create a temporary directory.
        let dir = tempdir().unwrap();
        let sub_dir = dir.path().join("sub_dir");
        fs::create_dir(&sub_dir).unwrap();
        let file1 = dir.path().join("file1.txt");
        let file2 = sub_dir.join("file2.txt");
        let mut f1 = File::create(&file1).unwrap();
        writeln!(f1, "File 1").unwrap();
        let mut f2 = File::create(&file2).unwrap();
        writeln!(f2, "File 2").unwrap();

        // Initialize DesignerState.
        let state = web::Data::new(Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        }));

        // Configure the `list_dir` route.
        let app = test::init_service(
            App::new()
                .app_data(state.clone())
                .route("/api/designer/v1/dir-list", web::post().to(list_dir_endpoint)),
        )
        .await;

        // Construct the request.
        let req_path = dir.path().to_string_lossy().to_string();
        let req = test::TestRequest::post()
            .uri("/api/designer/v1/dir-list")
            .set_json(ListDirRequestPayload {
                path: req_path,
            })
            .to_request();
        let resp = test::call_service(&app, req).await;

        assert!(resp.status().is_success());

        let body = test::read_body(resp).await;
        let response: ApiResponse<DirListResponseData> = serde_json::from_slice(&body).unwrap();

        assert_eq!(response.status, Status::Ok);
        assert_eq!(response.data.entries.len(), 2);

        let mut entries = response.data.entries.iter().collect::<Vec<_>>();
        entries.sort_by_key(|e| &e.name);
        assert_eq!(entries[0].name, "file1.txt");
        assert_eq!(entries[0].path, file1.to_string_lossy());
        assert!(!entries[0].is_dir);
        assert_eq!(entries[1].name, "sub_dir");
        assert_eq!(entries[1].path, sub_dir.to_string_lossy());
        assert!(entries[1].is_dir);
    }

    #[actix_web::test]
    async fn test_list_dir_with_non_existing_path() {
        let state = web::Data::new(Arc::new(DesignerState {
            tman_config: Arc::new(tokio::sync::RwLock::new(TmanConfig::default())),
            storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
            out: Arc::new(Box::new(TmanOutputCli)),
            pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
            graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
            persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
        }));

        let app = test::init_service(
            App::new()
                .app_data(state.clone())
                .route("/api/designer/v1/dir-list", web::post().to(list_dir_endpoint)),
        )
        .await;

        // Construct an invalid path.
        let non_existing_path = "/path/to/nonexistent".to_string();

        let req = test::TestRequest::post()
            .uri("/api/designer/v1/dir-list")
            .set_json(ListDirRequestPayload {
                path: non_existing_path,
            })
            .to_request();

        let resp = test::call_service(&app, req).await;

        // The updated dir_list endpoint returns a proper error, not a JSON
        // response
        assert_eq!(resp.status(), actix_web::http::StatusCode::NOT_FOUND);
    }
}
