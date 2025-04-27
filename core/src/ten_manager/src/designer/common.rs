//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use ten_rust::pkg_info::manifest::api::{
    ManifestApiMsg, ManifestApiPropertyAttributes,
};

use super::graphs::nodes::{DesignerApiMsg, DesignerPropertyAttributes};

pub fn get_designer_property_hashmap_from_pkg(
    items: HashMap<String, ManifestApiPropertyAttributes>,
) -> HashMap<String, DesignerPropertyAttributes> {
    items.into_iter().map(|(k, v)| (k, v.into())).collect()
}

pub fn get_designer_api_msg_from_pkg(
    items: Vec<ManifestApiMsg>,
) -> Vec<DesignerApiMsg> {
    items.into_iter().map(|v| v.into()).collect()
}
