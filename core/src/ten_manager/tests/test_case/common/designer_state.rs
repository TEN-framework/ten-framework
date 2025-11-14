//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{collections::HashMap, sync::Arc};

use ten_manager::{
    designer::{storage::in_memory::TmanStorageInMemory, DesignerState},
    home::config::{read_config, TmanConfig},
    output::cli::TmanOutputCli,
};

use super::tman_config::find_config_json;

pub fn create_designer_state() -> Arc<DesignerState> {
    let tman_config_file_path = find_config_json().map(|p| p.to_string_lossy().into_owned());

    let tman_config_file = read_config(&tman_config_file_path).unwrap();

    let default_tman_config = TmanConfig::default();

    let tman_config = TmanConfig {
        config_file: tman_config_file_path,
        registry: if let Some(tman_config_file) = &tman_config_file {
            tman_config_file.registry.clone()
        } else {
            // Use default registry instead of empty HashMap
            default_tman_config.registry
        },
        ..Default::default()
    };

    // Setup designer state
    let designer_state = DesignerState {
        tman_config: Arc::new(tokio::sync::RwLock::new(tman_config)),
        storage_in_memory: Arc::new(tokio::sync::RwLock::new(TmanStorageInMemory::default())),
        out: Arc::new(Box::new(TmanOutputCli)),
        pkgs_cache: tokio::sync::RwLock::new(HashMap::new()),
        graphs_cache: tokio::sync::RwLock::new(HashMap::new()),
        persistent_storage_schema: Arc::new(tokio::sync::RwLock::new(None)),
    };

    Arc::new(designer_state)
}
