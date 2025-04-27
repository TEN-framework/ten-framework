//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod add;
mod add_with_msg_conversion;
mod modify_msg_conversion;

use ten_rust::{
    graph::node::GraphNode,
    pkg_info::{pkg_type::PkgType, pkg_type_and_name::PkgTypeAndName},
};

pub fn create_test_node(
    name: &str,
    addon: &str,
    app: Option<&str>,
) -> GraphNode {
    GraphNode {
        type_and_name: PkgTypeAndName {
            pkg_type: PkgType::Extension,
            name: name.to_string(),
        },
        addon: addon.to_string(),
        extension_group: None,
        app: app.map(|s| s.to_string()),
        property: None,
    }
}
