//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod get;
pub mod get_schema;
pub mod update;
pub mod update_field;

use serde::{Deserialize, Serialize};

use crate::{
    config::{TmanConfig, TmanConfigFile},
    constants::DESIGNER_FRONTEND_DEFAULT_LOGVIEWER_LINE_SIZE,
};

use super::locale::Locale;

#[derive(Serialize, Deserialize, Debug, Clone, Default)]
pub struct Designer {
    pub logviewer_line_size: usize,
    pub locale: Locale,
}

pub fn default_designer() -> Designer {
    Designer {
        logviewer_line_size: DESIGNER_FRONTEND_DEFAULT_LOGVIEWER_LINE_SIZE,
        locale: Locale::default(),
    }
}

/// Helper function to save config to file.
pub fn save_config_to_file(
    tman_config: &mut TmanConfig,
) -> Result<(), actix_web::Error> {
    if let Some(config_file) = &tman_config.config_file {
        // Create TmanConfigFile structure for serialization.
        let config_file_content = TmanConfigFile {
            registry: tman_config.registry.clone(),
            admin_token: tman_config.admin_token.clone(),
            user_token: tman_config.user_token.clone(),
            enable_package_cache: tman_config.enable_package_cache,
            designer: tman_config.designer.clone(),
        };

        // Serialize and write to file.
        let config_json = serde_json::to_value(config_file_content)
            .map_err(actix_web::error::ErrorInternalServerError)?;

        std::fs::write(
            config_file,
            serde_json::to_string_pretty(&config_json)
                .map_err(actix_web::error::ErrorInternalServerError)?,
        )
        .map_err(actix_web::error::ErrorInternalServerError)?;
    }

    Ok(())
}
