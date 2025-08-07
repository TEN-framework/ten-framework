//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::constants::{
    ERR_MSG_GRAPH_APP_FIELD_SHOULD_BE_DECLARED,
    ERR_MSG_GRAPH_APP_FIELD_SHOULD_NOT_BE_DECLARED,
    ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_MULTI_APP_MODE,
};
use crate::pkg_info::localhost;
use crate::{
    constants::ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_SINGLE_APP_MODE,
    graph::is_app_default_loc_or_none,
};

use super::{msg_conversion::MsgAndResultConversion, AppUriDeclarationState};

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq, Hash)]
pub struct GraphLoc {
    #[serde(skip_serializing_if = "is_app_default_loc_or_none")]
    pub app: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub extension: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub subgraph: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub selector: Option<String>,
}

impl GraphLoc {
    pub fn new() -> Self {
        Self { app: None, extension: None, subgraph: None, selector: None }
    }

    pub fn with_app_and_extension_or_subgraph(
        app: Option<String>,
        extension: Option<String>,
        subgraph: Option<String>,
    ) -> Self {
        Self { app, extension, subgraph, selector: None }
    }

    pub fn get_app_uri(&self) -> &Option<String> {
        &self.app
    }

    /// Validates the app field according to the graph's app declaration rules.
    pub fn validate_app_field(
        &self,
        app_uri_declaration_state: &AppUriDeclarationState,
    ) -> Result<()> {
        if self.selector.is_some() || self.subgraph.is_some() {
            // Selector and subgraph do not have app field.
            return Ok(());
        }

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

            // If no nodes have declared app, locations shouldn't either.
            if *app_uri_declaration_state
                == AppUriDeclarationState::NoneDeclared
            {
                return Err(anyhow::anyhow!(
                    ERR_MSG_GRAPH_APP_FIELD_SHOULD_NOT_BE_DECLARED
                ));
            }
        } else {
            // If nodes have declared app, locations must also declare it.
            if *app_uri_declaration_state
                != AppUriDeclarationState::NoneDeclared
            {
                return Err(anyhow::anyhow!(
                    ERR_MSG_GRAPH_APP_FIELD_SHOULD_BE_DECLARED
                ));
            }
        }

        Ok(())
    }
}

impl Default for GraphLoc {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphConnection {
    #[serde(flatten)]
    pub loc: GraphLoc,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub cmd: Option<Vec<GraphMessageFlow>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub data: Option<Vec<GraphMessageFlow>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_frame: Option<Vec<GraphMessageFlow>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_frame: Option<Vec<GraphMessageFlow>>,
}

impl GraphConnection {
    pub fn new(
        app: Option<String>,
        extension: Option<String>,
        subgraph: Option<String>,
    ) -> Self {
        Self {
            loc: GraphLoc::with_app_and_extension_or_subgraph(
                app, extension, subgraph,
            ),
            cmd: None,
            data: None,
            audio_frame: None,
            video_frame: None,
        }
    }

    /// Validates and completes a graph connection by ensuring it follows the
    /// app declaration rules of the graph and validating all message flows.
    pub fn validate_and_complete(
        &mut self,
        app_uri_declaration_state: &AppUriDeclarationState,
    ) -> Result<()> {
        // Validate the location's app field.
        self.loc.validate_app_field(app_uri_declaration_state)?;

        // Validate all message flows.
        if let Some(cmd) = &mut self.cmd {
            for (idx, cmd_flow) in cmd.iter_mut().enumerate() {
                cmd_flow
                    .validate_and_complete(app_uri_declaration_state)
                    .map_err(|e| anyhow::anyhow!("cmd[{}]: {}", idx, e))?;
            }
        }

        if let Some(data) = &mut self.data {
            for (idx, data_flow) in data.iter_mut().enumerate() {
                data_flow
                    .validate_and_complete(app_uri_declaration_state)
                    .map_err(|e| anyhow::anyhow!("data[{}]: {}", idx, e))?;
            }
        }

        if let Some(audio_frame) = &mut self.audio_frame {
            for (idx, audio_flow) in audio_frame.iter_mut().enumerate() {
                audio_flow
                    .validate_and_complete(app_uri_declaration_state)
                    .map_err(|e| {
                        anyhow::anyhow!("audio_frame[{}]: {}", idx, e)
                    })?;
            }
        }

        if let Some(video_frame) = &mut self.video_frame {
            for (idx, video_flow) in video_frame.iter_mut().enumerate() {
                video_flow
                    .validate_and_complete(app_uri_declaration_state)
                    .map_err(|e| {
                        anyhow::anyhow!("video_frame[{}]: {}", idx, e)
                    })?;
            }
        }

        Ok(())
    }

    pub fn get_app_uri(&self) -> &Option<String> {
        self.loc.get_app_uri()
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphMessageFlow {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub names: Option<Vec<String>>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dest: Vec<GraphDestination>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub source: Vec<GraphSource>,
}

impl GraphMessageFlow {
    /// Validates the mutual exclusivity of name and names fields.
    ///
    /// Ensures that exactly one of 'name' or 'names' is specified, but not both
    /// and not neither.
    pub fn validate_name_mutual_exclusivity(&self) -> Result<()> {
        match (&self.name, &self.names) {
            (Some(_), Some(_)) => Err(anyhow::anyhow!(
                "Both 'name' and 'names' fields are specified, but they are \
                 mutually exclusive"
            )),
            (None, None) => Err(anyhow::anyhow!(
                "Neither 'name' nor 'names' field is specified, but one must \
                 be provided"
            )),
            (None, Some(names)) if names.is_empty() => Err(anyhow::anyhow!(
                "'names' field is empty, must contain at least one name"
            )),
            _ => Ok(()),
        }
    }

    /// Validates and completes a message flow by ensuring all destinations are
    /// properly configured.
    ///
    /// This method performs the following steps:
    /// 1. Iterates through all destinations in the message flow.
    /// 2. Validates and completes each destination with the app declaration
    ///    context.
    ///
    /// # Arguments
    /// * `graph_node_app_declaration` - The app declaration state of the entire
    ///   graph (NoneDeclared, UniformDeclared, or MixedDeclared).
    ///
    /// # Returns
    /// * `Ok(())` if validation succeeds for all destinations.
    /// * `Err` with a descriptive error message if validation fails, including
    ///   the index of the problematic destination.
    fn validate_and_complete(
        &mut self,
        app_uri_declaration_state: &AppUriDeclarationState,
    ) -> Result<()> {
        // First validate name/names mutual exclusivity
        self.validate_name_mutual_exclusivity()?;

        for (idx, dest) in self.dest.iter_mut().enumerate() {
            dest.validate_and_complete(app_uri_declaration_state)
                .map_err(|e| anyhow::anyhow!("dest[{}]: {}", idx, e))?;
        }

        Ok(())
    }

    pub fn new(
        name: String,
        dest: Vec<GraphDestination>,
        source: Vec<GraphSource>,
    ) -> Self {
        Self { name: Some(name), names: None, dest, source }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphDestination {
    #[serde(flatten)]
    pub loc: GraphLoc,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub msg_conversion: Option<MsgAndResultConversion>,
}

impl GraphDestination {
    pub fn new(
        app: Option<String>,
        extension: Option<String>,
        subgraph: Option<String>,
    ) -> Self {
        Self {
            loc: GraphLoc::with_app_and_extension_or_subgraph(
                app, extension, subgraph,
            ),
            msg_conversion: None,
        }
    }

    /// Validates and completes a destination by ensuring it follows the app
    /// declaration rules and has valid message conversion if specified.
    ///
    /// For graphs spanning multiple apps, no destination can have 'localhost'
    /// as its app field value, as other apps would not know how to connect to
    /// the app that destination belongs to. For consistency, single app graphs
    /// also do not allow 'localhost' as an explicit app field value. Instead,
    /// 'localhost' is used as the internal default value when no app field is
    /// specified.
    fn validate_and_complete(
        &mut self,
        app_uri_declaration_state: &AppUriDeclarationState,
    ) -> Result<()> {
        // Validate the location's app field.
        self.loc.validate_app_field(app_uri_declaration_state)?;

        // Validate message conversion configuration if present.
        if let Some(msg_conversion) = &self.msg_conversion {
            msg_conversion.validate().map_err(|e| {
                anyhow::anyhow!("invalid msg_conversion: {}", e)
            })?;
        }

        Ok(())
    }

    pub fn get_app_uri(&self) -> &Option<String> {
        self.loc.get_app_uri()
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GraphSource {
    #[serde(flatten)]
    pub loc: GraphLoc,
}
