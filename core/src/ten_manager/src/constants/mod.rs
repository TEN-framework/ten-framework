//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod registry;
pub mod tag;

pub use registry::*;
pub use tag::*;

pub const DOT_TEN_DIR: &str = ".ten";
pub const APP_DIR_IN_DOT_TEN_DIR: &str = "app";
pub const INSTALLED_PATHS_JSON_FILENAME: &str = "installed_paths.json";
pub const PACKAGE_INFO_DIR_IN_DOT_TEN_DIR: &str = "package_info";
pub const PACKAGE_DIR_IN_DOT_TEN_DIR: &str = "package";

pub const LIB_DIR: &str = "lib";
pub const INCLUDE_DIR: &str = "include";

pub const APP_PKG_TYPE: &str = "app";

pub const SCRIPTS: &str = "scripts";

pub const INSTALL_PATHS_APP_PREFIX: &str = "@app";

pub const TEN_PACKAGE_FILE_EXTENSION: &str = ".tpkg";

// Maximum number of retry attempts.
pub const REMOTE_REGISTRY_MAX_RETRIES: u32 = 30;
// Delay between retries in milliseconds.
pub const REMOTE_REGISTRY_RETRY_DELAY_MS: u64 = 500;
// Timeout duration for requests in seconds.
pub const REMOTE_REGISTRY_REQUEST_TIMEOUT_SECS: u64 = 120;

pub const GITHUB_RELEASE_REQUEST_TIMEOUT_SECS: u64 = 10;

pub const DEFAULT_REGISTRY_PAGE_SIZE: u32 = 100;

// Designer-backend.
pub const PROCESS_STDOUT_MAX_LINE_CNT: usize = 1000;
pub const PROCESS_STDERR_MAX_LINE_CNT: usize = 1000;

// Designer frontend.
pub const DESIGNER_FRONTEND_DEFAULT_LOGVIEWER_LINE_SIZE: usize = 1000;

pub const DESIGNER_BACKEND_DEFAULT_PORT: &str = "49483";

pub const TEST_DIR: &str = "test_dir";

pub const DEFAULT: &str = "default";

// Template package names.
pub const DEFAULT_APP_CPP: &str = "default_app_cpp";
pub const DEFAULT_APP_GO: &str = "default_app_go";
pub const DEFAULT_APP_PYTHON: &str = "default_app_python";
pub const DEFAULT_APP_NODEJS: &str = "default_app_nodejs";
pub const DEFAULT_EXTENSION_CPP: &str = "default_extension_cpp";
pub const DEFAULT_EXTENSION_GO: &str = "default_extension_go";
pub const DEFAULT_EXTENSION_PYTHON: &str = "default_extension_python";
pub const DEFAULT_EXTENSION_NODEJS: &str = "default_extension_nodejs";

pub const TEN_NAME_RULE_PATH: &str = "ten.name";

pub const PACKAGE_CACHE: &str = "package_cache";
pub const CONFIG_JSON: &str = "config.json";
pub const DATA_JSON: &str = "data.json";
pub const METADATA_FILE: &str = "metadata.json";

pub const BUF_WRITER_BUF_SIZE: usize = 1024 * 1024;

pub const DEFAULT_INIT_LATEST_VERSIONS_WHEN_INSTALL: i32 = 3;
pub const DEFAULT_MAX_LATEST_VERSIONS_WHEN_INSTALL: i32 = 9;
pub const DEFAULT_MAX_LATEST_VERSIONS_WHEN_INSTALL_STR: &str = "9";
