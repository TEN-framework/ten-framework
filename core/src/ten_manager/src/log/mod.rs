//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;
use std::collections::HashMap;

pub struct ExtensionInfo {
    pub thread_id: u64,
}

pub struct GraphResourcesLog {
    pub app_uri: Option<String>,
    pub graph_id: String,
    pub graph_name: String,
    pub extensions: HashMap<String, ExtensionInfo>,
}

pub fn parse_graph_resources_log(
    log_message: &str,
    graph_resources_log: &mut GraphResourcesLog,
) -> Result<()> {
    Ok(())
}
