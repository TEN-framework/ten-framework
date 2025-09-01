//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
use std::{collections::HashMap, fs, path::PathBuf, sync::Arc};

use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::{
    constants::{CONFIG_JSON, DEFAULT, DEFAULT_REGISTRY},
    designer::preferences::{default_designer, Designer},
    home::{get_home_dir, package_cache::default_enable_package_cache, Registry},
    schema::validate_tman_config,
};

#[derive(Serialize, Deserialize, Debug, Default)]
pub struct TmanConfigFile {
    pub registry: HashMap<String, Registry>,

    pub admin_token: Option<String>,
    pub user_token: Option<String>,

    #[serde(default = "default_enable_package_cache")]
    pub enable_package_cache: bool,

    #[serde(default = "default_designer")]
    pub designer: Designer,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct TmanConfig {
    pub registry: HashMap<String, Registry>,

    pub config_file: Option<String>,
    pub admin_token: Option<String>,
    pub user_token: Option<String>,

    pub verbose: bool,
    pub assume_yes: bool,

    pub enable_package_cache: bool,

    pub designer: Designer,
}

impl Default for TmanConfig {
    fn default() -> Self {
        let mut registry = HashMap::new();

        registry.entry(DEFAULT.to_string()).or_insert(Registry {
            index: DEFAULT_REGISTRY.to_string(),
        });

        Self {
            registry,
            config_file: Some(get_home_config_path().to_string_lossy().to_string()),
            admin_token: None,
            user_token: None,
            verbose: false,
            assume_yes: false,
            enable_package_cache: true,
            designer: default_designer(),
        }
    }
}

/// Get the default configuration file path, located under the default home
/// directory as `config.json`.
pub fn get_home_config_path() -> PathBuf {
    let mut config_path = get_home_dir();
    config_path.push(CONFIG_JSON);
    config_path
}

// Read the configuration from the specified path.
pub fn read_config(config_file_path: &Option<String>) -> Result<Option<TmanConfigFile>> {
    let config_path = match config_file_path {
        Some(path) => PathBuf::from(path),
        None => get_home_config_path(),
    };

    let config_data = fs::read_to_string(config_path.clone());

    if config_path.exists() {
        match config_data {
            Ok(data) => match serde_json::from_str(&data) {
                Ok(config_json) => {
                    // Validate the config against schema.
                    if let Err(e) = validate_tman_config(&config_json) {
                        return Err(anyhow::anyhow!("Failed to validate config file: {}", e));
                    }

                    // Parse the config.
                    match serde_json::from_value::<TmanConfigFile>(config_json) {
                        Ok(config) => Ok(Some(config)),
                        Err(e) => Err(anyhow::anyhow!("Failed to parse config file: {}", e)),
                    }
                }
                Err(e) => Err(anyhow::anyhow!("Failed to parse config file: {}", e)),
            },
            Err(e) => Err(anyhow::anyhow!("Failed to read config file: {}", e)),
        }
    } else {
        Ok(None)
    }
}

pub async fn is_verbose(tman_config: Arc<tokio::sync::RwLock<TmanConfig>>) -> bool {
    tman_config.read().await.verbose
}
