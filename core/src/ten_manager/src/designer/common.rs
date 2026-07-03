//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use ten_rust::pkg_info::manifest::api::{ManifestApiMsg, ManifestApiProperty};

use super::graphs::nodes::{DesignerApiMsg, DesignerApiProperty};

pub fn get_designer_api_property_from_pkg(items: ManifestApiProperty) -> DesignerApiProperty {
    items.into()
}

pub fn get_designer_api_msg_from_pkg(items: Vec<ManifestApiMsg>) -> Vec<DesignerApiMsg> {
    items.into_iter().map(|v| v.into()).collect()
}
