//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// The `ten_manager` crate is only a binary crate, and does not need to provide
// a `lib.rs` file. However, we can _NOT_ write integration tests (test cases in
// the `tests` folder) for `ten_manager` if it does not have a `lib.rs` file. In
// other words, `use` directive does not work for binary crates. According to
// the official reference, we can provide this `lib.rs` file to export the
// important functions for integration tests, and the `main.rs` should be a thin
// wrapper around the library crate, to provide the binary functionality. Refer
// to: https://doc.rust-lang.org/book/ch11-03-test-organization.html#integration-tests-for-binary-crates.
//
// The `lib.rs` does not affect the final output of the binary crate, as the
// output name of the library is different from the binary crate.
//
// Because of the existence of this `lib.rs` file, the unit test will be
// compiled with the `lib.rs`, but not `main.rs`, and some common settings such
// as the allocator below should be added to the `lib.rs` file.
pub mod cmd;
pub mod cmd_line;
pub mod config;
pub mod constants;
mod create;
mod dep_and_candidate;
pub mod designer;
mod file_type;
mod fs;
pub mod graph;
pub mod http;
mod install;
pub mod json;
mod manifest_lock;
pub mod output;
mod package_file;
pub mod pkg_info;
pub mod registry;
pub mod schema;
mod solver;
pub mod version;
pub mod version_utils;

#[cfg(not(target_os = "windows"))]
use mimalloc::MiMalloc;

// TODO(Wei): When adding a URL route with variables (e.g., /api/{name}) in
// actix-web, using the default allocator can lead to a memory leak. According
// to the information in the internet, this leak is likely a false positive and
// may be related to the caching mechanism in actix-web. However, if we use
// mimalloc or jemalloc, there won't be any leak. Use this method to avoid the
// issue for now and study it in more detail in the future.
//
// Refer to the following posts:
// https://github.com/hyperium/hyper/issues/1790#issuecomment-2170644852
// https://github.com/actix/actix-web/issues/1780
// https://news.ycombinator.com/item?id=21962195
#[cfg(not(target_os = "windows"))]
#[global_allocator]
static GLOBAL: MiMalloc = MiMalloc;
