//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod add;
pub mod delete;
pub mod property;
pub mod replace;

use std::collections::HashMap;

use anyhow::Result;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use ten_rust::base_dir_pkg_info::PkgsInfoInApp;
use ten_rust::graph::graph_info::GraphInfo;
use ten_rust::graph::node::{AtomicFilter, Filter, FilterOperator, GraphNode};
use ten_rust::pkg_info::manifest::api::ManifestApiMsg;
use ten_rust::pkg_info::manifest::api::{
    ManifestApiCmdResult, ManifestApiProperty, ManifestApiPropertyAttributes,
};
use ten_rust::pkg_info::value_type::ValueType;

use crate::designer::graphs::DesignerGraph;
use crate::graph::update_graph_node_all_fields;
use crate::pkg_info::belonging_pkg_info_find_by_graph_info_mut;

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerApiProperty {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub properties: Option<HashMap<String, DesignerPropertyAttributes>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<Vec<String>>,
}

impl From<ManifestApiProperty> for DesignerApiProperty {
    fn from(manifest_property: ManifestApiProperty) -> Self {
        let properties_map = manifest_property.properties().map(|properties| {
            properties
                .iter()
                .map(|(k, v)| (k.clone(), v.clone().into()))
                .collect()
        });

        DesignerApiProperty {
            properties: properties_map,
            required: manifest_property
                .required
                .as_ref()
                .filter(|req| !req.is_empty())
                .cloned(),
        }
    }
}

impl DesignerApiProperty {
    pub fn len(&self) -> usize {
        self.properties.as_ref().map_or(0, |p| p.len())
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn get(&self, key: &str) -> Option<&DesignerPropertyAttributes> {
        self.properties.as_ref()?.get(key)
    }
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerApi {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<DesignerApiProperty>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub cmd_in: Option<Vec<DesignerApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub cmd_out: Option<Vec<DesignerApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub data_in: Option<Vec<DesignerApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub data_out: Option<Vec<DesignerApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_frame_in: Option<Vec<DesignerApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_frame_out: Option<Vec<DesignerApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_frame_in: Option<Vec<DesignerApiMsg>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_frame_out: Option<Vec<DesignerApiMsg>>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct DesignerPropertyAttributes {
    #[serde(rename = "type")]
    pub prop_type: ValueType,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub items: Option<Box<DesignerPropertyAttributes>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub properties: Option<HashMap<String, DesignerPropertyAttributes>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<Vec<String>>,
}

impl From<ManifestApiPropertyAttributes> for DesignerPropertyAttributes {
    fn from(api_property: ManifestApiPropertyAttributes) -> Self {
        DesignerPropertyAttributes {
            prop_type: api_property.prop_type,
            items: api_property.items.map(|items| Box::new((*items).into())),
            properties: api_property.properties.map(|props| {
                props.into_iter().map(|(k, v)| (k, v.into())).collect()
            }),
            required: api_property.required,
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct DesignerCmdResult {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<DesignerApiProperty>,
}

impl From<ManifestApiCmdResult> for DesignerCmdResult {
    fn from(cmd_result: ManifestApiCmdResult) -> Self {
        DesignerCmdResult { property: cmd_result.property.map(Into::into) }
    }
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerApiMsg {
    pub name: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<DesignerApiProperty>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub result: Option<DesignerCmdResult>,
}

impl From<ManifestApiMsg> for DesignerApiMsg {
    fn from(api_cmd_like: ManifestApiMsg) -> Self {
        DesignerApiMsg {
            name: api_cmd_like.name,
            property: api_cmd_like
                .property
                .as_ref()
                .filter(|p| !p.is_empty())
                .cloned()
                .map(Into::into),
            result: api_cmd_like.result.as_ref().cloned().map(Into::into),
        }
    }
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerExtensionNode {
    pub addon: String,
    pub name: String,

    // The app which this extension belongs.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub app: Option<String>,

    // The extension group which this extension belongs.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension_group: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub api: Option<DesignerApi>,

    pub property: Option<serde_json::Value>,

    /// This indicates that the extension has been installed under the
    /// `ten_packages/` directory.
    pub is_installed: bool,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerSubgraphNode {
    pub name: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub property: Option<serde_json::Value>,

    pub graph: DesignerGraphContent,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerGraphContent {
    pub import_uri: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub graph: Option<DesignerGraph>,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerSelectorNode {
    pub name: String,
    pub filter: DesignerFilter,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum DesignerFilterOperator {
    #[serde(rename = "exact")]
    Exact,
    #[serde(rename = "regex")]
    Regex,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub struct DesignerAtomicFilter {
    pub field: String,
    pub operator: DesignerFilterOperator,
    pub value: String,
}

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
#[serde(untagged)]
pub enum DesignerFilter {
    Atomic(DesignerAtomicFilter),
    And { and: Vec<DesignerFilter> },
    Or { or: Vec<DesignerFilter> },
}

impl From<FilterOperator> for DesignerFilterOperator {
    fn from(op: FilterOperator) -> Self {
        match op {
            FilterOperator::Exact => DesignerFilterOperator::Exact,
            FilterOperator::Regex => DesignerFilterOperator::Regex,
        }
    }
}

impl From<AtomicFilter> for DesignerAtomicFilter {
    fn from(filter: AtomicFilter) -> Self {
        DesignerAtomicFilter {
            field: filter.field,
            operator: filter.operator.into(),
            value: filter.value,
        }
    }
}

impl From<Filter> for DesignerFilter {
    fn from(filter: Filter) -> Self {
        match filter {
            Filter::Atomic(atomic) => DesignerFilter::Atomic(atomic.into()),
            Filter::And { and } => DesignerFilter::And {
                and: and.into_iter().map(|f| f.into()).collect(),
            },
            Filter::Or { or } => DesignerFilter::Or {
                or: or.into_iter().map(|f| f.into()).collect(),
            },
        }
    }
}

/// Represents a node in a designer graph. This enum represents different types
/// of nodes that can exist in the graph, similar to GraphNode but designed for
/// the designer API.
#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum DesignerGraphNode {
    Extension {
        #[serde(flatten)]
        content: Box<DesignerExtensionNode>,
    },
    Subgraph {
        #[serde(flatten)]
        content: Box<DesignerSubgraphNode>,
    },
    Selector {
        #[serde(flatten)]
        content: Box<DesignerSelectorNode>,
    },
}

impl DesignerGraphNode {
    /// Get the name of the node regardless of its type.
    pub fn get_name(&self) -> &str {
        match self {
            DesignerGraphNode::Extension { content } => &content.name,
            DesignerGraphNode::Subgraph { content } => &content.name,
            DesignerGraphNode::Selector { content } => &content.name,
        }
    }
}

impl TryFrom<GraphNode> for DesignerGraphNode {
    type Error = anyhow::Error;

    fn try_from(node: GraphNode) -> Result<Self, Self::Error> {
        match node {
            GraphNode::Extension { content } => {
                Ok(DesignerGraphNode::Extension {
                    content: Box::new(DesignerExtensionNode {
                        addon: content.addon,
                        name: content.name,
                        extension_group: content.extension_group,
                        app: content.app,
                        api: None,
                        property: content.property,
                        is_installed: false,
                    }),
                })
            }
            GraphNode::Subgraph { content } => {
                Ok(DesignerGraphNode::Subgraph {
                    content: Box::new(DesignerSubgraphNode {
                        name: content.name,
                        property: content.property,
                        graph: DesignerGraphContent {
                            import_uri: content.graph.import_uri,
                            // Will be populated during graph resolution
                            graph: None,
                        },
                    }),
                })
            }
            GraphNode::Selector { content } => {
                Ok(DesignerGraphNode::Selector {
                    content: Box::new(DesignerSelectorNode {
                        name: content.name,
                        filter: DesignerFilter::from(content.filter),
                    }),
                })
            }
        }
    }
}

/// Retrieves all extension nodes from a specified graph.
pub fn get_nodes_in_graph<'a>(
    graph_id: &Uuid,
    graphs_cache: &'a HashMap<Uuid, GraphInfo>,
) -> Result<&'a Vec<GraphNode>> {
    // Look for the graph by ID in the graphs_cache.
    if let Some(graph_info) = graphs_cache.get(graph_id) {
        // Collect all extension nodes from the graph.
        Ok(graph_info.graph.nodes())
    } else {
        Err(anyhow::anyhow!(
            "Graph with ID '{}' not found in graph caches",
            graph_id
        ))
    }
}

pub enum GraphNodeUpdateAction {
    Add,
    Delete,
    Update,
}

#[allow(clippy::too_many_arguments)]
pub fn update_graph_node_in_property_all_fields(
    pkgs_cache: &mut HashMap<String, PkgsInfoInApp>,
    graph_info: &mut GraphInfo,
    node_name: &str,
    addon_name: &str,
    extension_group_name: &Option<String>,
    app_uri: &Option<String>,
    property: &Option<serde_json::Value>,
    action: GraphNodeUpdateAction,
) -> Result<()> {
    if let Ok(Some(pkg_info)) =
        belonging_pkg_info_find_by_graph_info_mut(pkgs_cache, graph_info)
    {
        // Create the graph node.
        let new_node = GraphNode::new_extension_node(
            node_name.to_string(),
            addon_name.to_string(),
            extension_group_name.clone(),
            app_uri.clone(),
            property.clone(),
        );

        // Update property.json file with the graph node.
        if let Some(property) = &mut pkg_info.property {
            // Write the updated property_all_fields map to property.json.
            let nodes_to_updating = vec![new_node.clone()];

            // Determine which parameter to use based on action.
            let nodes_to_add = match &action {
                GraphNodeUpdateAction::Add => {
                    Some(nodes_to_updating.as_slice())
                }
                _ => None,
            };

            let nodes_to_remove = match &action {
                GraphNodeUpdateAction::Delete => {
                    Some(nodes_to_updating.as_slice())
                }
                _ => None,
            };

            let nodes_to_modify_property = match &action {
                GraphNodeUpdateAction::Update => {
                    Some(nodes_to_updating.as_slice())
                }
                _ => None,
            };

            if let Err(e) = update_graph_node_all_fields(
                &pkg_info.url,
                &mut property.all_fields,
                graph_info.name.as_ref().unwrap(),
                nodes_to_add,
                nodes_to_remove,
                nodes_to_modify_property,
            ) {
                eprintln!("Warning: Failed to update property.json file: {e}");
            }
        }
    }

    Ok(())
}
