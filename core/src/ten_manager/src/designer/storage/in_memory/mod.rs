//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod get;
pub mod key_parser;
pub mod set;

use serde::{Deserialize, Serialize};
use serde_json;

#[derive(Serialize, Deserialize, Debug, Default, Clone)]
pub struct TmanStorageInMemory {
    #[serde(flatten)]
    pub extra_fields: serde_json::Map<String, serde_json::Value>,
}
