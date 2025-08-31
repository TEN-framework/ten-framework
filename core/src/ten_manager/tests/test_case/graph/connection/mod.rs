//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod add;
mod add_with_msg_conversion;

use ten_rust::graph::node::GraphNode;

pub fn create_test_node(name: &str, addon: &str, app: Option<&str>) -> GraphNode {
    GraphNode::new_extension_node(
        name.to_string(),
        addon.to_string(),
        None,
        app.map(|s| s.to_string()),
        None,
    )
}
