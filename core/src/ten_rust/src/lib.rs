//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod backtrace;
pub mod base_dir_pkg_info;
pub mod bindings;
pub mod constants;
pub mod crypto;
pub mod env;
pub mod graph;
pub mod json;
pub mod json_schema;
pub mod log;
pub mod pkg_info;
pub mod utils;

#[cfg(feature = "service-hub")]
pub mod service_hub;

// In the schema/ folder, the Rust API is mainly automatically generated from C
// headers. There's no need to generate C code in reverse from Rust.
// Additionally, if including schema/ folder here, it would result in duplicate
// declarations.
/// cbindgen:ignore
pub mod schema;
