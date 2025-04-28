//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::env;

use crate::pkg_info::constants::TEN_FIELD_IN_PROPERTY;

pub fn get_ten_field_string() -> String {
    match env::var("TMAN_08_COMPATIBLE") {
        Ok(val) if val == "true" => "_ten".to_string(),
        _ => TEN_FIELD_IN_PROPERTY.to_string(),
    }
}
