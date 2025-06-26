//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use anyhow::{anyhow, Context, Result};
use serde::{Deserialize, Serialize};

use crate::pkg_info::pkg_type::PkgType;
use crate::utils::path::{get_base_dir_of_uri, get_real_path_from_import_uri};
use crate::utils::uri::load_content_from_uri;

use super::Graph;

/// Loads graph data from the specified URI with an optional base directory.
///
/// The URI can be:
/// - A relative path (relative to the base_dir if provided)
/// - A URI (http:// or https:// or file://)
///
/// TODO(Wei): Absolute file paths are NOT supported. Use file:// URI instead.
/// According to the uri-reference specification, absolute file paths require
/// special handling. For example, on Windows, absolute paths need to start with
/// a forward slash, like /c:/..., so simply using Path::new(uri).is_absolute()
/// is insufficient and requires additional consideration.
///
/// This function returns the loaded Graph structure.
pub async fn load_graph_from_uri(
    uri: &str,
    base_dir: Option<&str>,
    new_base_dir: &mut Option<String>,
) -> Result<Graph> {
    // Get the real path of the import_uri based on the base_dir.
    let real_path = get_real_path_from_import_uri(uri, base_dir)?;

    // Read the graph file.
    let graph_content = load_content_from_uri(&real_path).await?;

    *new_base_dir = Some(get_base_dir_of_uri(&real_path)?);

    // Parse the graph file into a Graph structure.
    let graph: Graph =
        serde_json::from_str(&graph_content).with_context(|| {
            format!("Failed to parse graph file from {real_path}")
        })?;

    Ok(graph)
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphInfo {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub auto_start: Option<bool>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub singleton: Option<bool>,

    #[serde(flatten)]
    pub graph: Graph,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub import_uri: Option<String>,

    #[serde(skip)]
    pub app_base_dir: Option<String>,
    #[serde(skip)]
    pub belonging_pkg_type: Option<PkgType>,
    #[serde(skip)]
    pub belonging_pkg_name: Option<String>,
}

impl GraphInfo {
    pub async fn from_str_with_base_dir(
        s: &str,
        current_base_dir: Option<&str>,
    ) -> Result<Self> {
        let mut graph_info: GraphInfo = serde_json::from_str(s)?;
        graph_info.app_base_dir = current_base_dir.map(|s| s.to_string());
        graph_info.validate_and_complete_and_flatten().await?;
        // Return the parsed data.
        Ok(graph_info)
    }

    pub async fn validate_and_complete_and_flatten(&mut self) -> Result<()> {
        // Validate mutual exclusion between import_uri and graph fields
        if self.import_uri.is_some() {
            // When import_uri is present, the graph fields should be empty or
            // None
            if !self.graph.nodes.is_empty() {
                return Err(anyhow!(
                    "When 'import_uri' is specified, 'nodes' field must not \
                     be present"
                ));
            }

            if let Some(connections) = &self.graph.connections {
                if !connections.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'connections' field \
                         must not be present"
                    ));
                }
            }

            if let Some(exposed_messages) = &self.graph.exposed_messages {
                if !exposed_messages.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'exposed_messages' \
                         field must not be present"
                    ));
                }
            }

            if let Some(exposed_properties) = &self.graph.exposed_properties {
                if !exposed_properties.is_empty() {
                    return Err(anyhow!(
                        "When 'import_uri' is specified, 'exposed_properties' \
                         field must not be present"
                    ));
                }
            }
        }

        // If import_uri is specified, load graph from the URI.
        let import_uri = self.import_uri.clone();
        let app_base_dir = self.app_base_dir.clone();
        if let Some(import_uri) = import_uri {
            // Load graph from URI and replace the current graph
            let graph = load_graph_from_uri(
                &import_uri,
                app_base_dir.as_deref(),
                &mut None,
            )
            .await?;
            self.graph = graph;
        }

        self.graph
            .validate_and_complete_and_flatten(app_base_dir.as_deref())
            .await
    }
}
