//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::constants::ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_MULTI_APP_MODE;
use crate::constants::ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_SINGLE_APP_MODE;
use crate::graph::AppUriDeclarationState;

use crate::graph::is_app_default_loc_or_none;
use crate::pkg_info::{localhost, pkg_type_and_name::PkgTypeAndName};

/// Represents a node in a graph. This struct is completely equivalent to the
/// node element in the graph JSON.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphNode {
    #[serde(flatten)]
    pub type_and_name: PkgTypeAndName,

    pub addon: String,

    /// The extension group this node belongs to. This field is only present
    /// for extension nodes. Extension group nodes themselves do not contain
    /// this field, as they define groups rather than belong to them.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,

    #[serde(skip_serializing_if = "is_app_default_loc_or_none")]
    pub app: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<serde_json::Value>,
}

impl GraphNode {
    /// Validates and completes a graph node by ensuring it has all required
    /// fields and follows the app declaration rules of the graph.
    ///
    /// For graphs spanning multiple apps, no node can have 'localhost' as its
    /// app field value, as other apps would not know how to connect to the
    /// app that node belongs to. For consistency, single app graphs also do
    /// not allow 'localhost' as an explicit app field value. Instead,
    /// 'localhost' is used as the internal default value when no app field is
    /// specified.
    pub fn validate_and_complete(
        &mut self,
        app_uri_declaration_state: &AppUriDeclarationState,
    ) -> Result<()> {
        // Check if app URI is provided and validate it.
        if let Some(app) = &self.app {
            // Disallow 'localhost' as an app URI in graph definitions.
            if app.as_str() == localhost() {
                let err_msg = if app_uri_declaration_state.is_single_app_graph()
                {
                    ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_SINGLE_APP_MODE
                } else {
                    ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_MULTI_APP_MODE
                };

                return Err(anyhow::anyhow!(err_msg));
            }
        }

        Ok(())
    }

    pub fn get_app_uri(&self) -> &Option<String> {
        &self.app
    }
}
