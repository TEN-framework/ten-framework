//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{env, sync::Mutex};

use tempfile::TempDir;

// # Problem
//
// When running tests in parallel, multiple test cases that modify the home
// directory environment need to avoid interfering with each other, causing
// race conditions and intermittent test failures.
//
// The issue occurs because:
// 1. Environment variables are global to the entire process
// 2. Multiple tests running concurrently can overwrite each other's home
//    directory settings
// 3. The cleanup code in `Drop` implementations may restore incorrect values
// 4. Different platforms use different environment variables for home
//    directory:
//    - Unix systems: HOME
//    - Windows: USERPROFILE
//
// # Solution
//
// A thread-safe solution using a global mutex to serialize access to a custom
// test-only environment variable `TEN_MANAGER_HOME_INTERNAL_USE_ONLY`. The
// `get_home_dir()` function checks for this variable first, making it work
// consistently across all platforms without interfering with system home
// directory detection.

// Use a global mutex to serialize access to TEN_MANAGER_HOME_INTERNAL_USE_ONLY
// environment variable across all tests
static HOME_MUTEX: Mutex<()> = Mutex::new(());

/// Helper struct to manage temporary home directory with thread-safe access.
///
/// This struct ensures that when multiple tests run in parallel, they won't
/// interfere with each other's TEN_MANAGER_HOME_INTERNAL_USE_ONLY environment
/// variable settings. The mutex ensures exclusive access to the
/// TEN_MANAGER_HOME_INTERNAL_USE_ONLY environment variable.
pub struct TempHome {
    _temp_dir: TempDir,
    _guard: std::sync::MutexGuard<'static, ()>,
}

impl TempHome {
    /// Create a new temporary home directory and set it as the
    /// TEN_MANAGER_HOME_INTERNAL_USE_ONLY environment variable.
    ///
    /// This will acquire a global mutex to ensure thread-safe access across
    /// parallel tests.
    pub fn new() -> Self {
        // Acquire the lock to ensure exclusive access to
        // TEN_MANAGER_HOME_INTERNAL_USE_ONLY environment variable
        let guard = HOME_MUTEX.lock().expect("Failed to lock HOME mutex");

        let temp_dir = TempDir::new().expect("Failed to create temp directory");
        env::set_var("TEN_MANAGER_HOME_INTERNAL_USE_ONLY", temp_dir.path());

        Self {
            _temp_dir: temp_dir,
            _guard: guard,
        }
    }

    /// Get the path to the temporary home directory
    #[allow(dead_code)]
    pub fn path(&self) -> &std::path::Path {
        self._temp_dir.path()
    }
}

impl Drop for TempHome {
    fn drop(&mut self) {
        // Restore original TEN_MANAGER_HOME_INTERNAL_USE_ONLY - the mutex guard
        // will be released automatically when this struct is dropped,
        // ensuring exclusive access during cleanup
        env::remove_var("TEN_MANAGER_HOME_INTERNAL_USE_ONLY");
        // _guard is dropped here, releasing the mutex
    }
}

/// Execute a closure with a temporary home directory set as
/// TEN_MANAGER_HOME_INTERNAL_USE_ONLY environment variable.
///
/// This function provides thread-safe access to
/// TEN_MANAGER_HOME_INTERNAL_USE_ONLY environment variable modification for
/// testing purposes.
///
/// # Automatic Cleanup
///
/// The temporary directory created by this function is automatically cleaned up
/// when the function returns. This is guaranteed by the `TempDir` type from the
/// `tempfile` crate, which implements `Drop` to remove the temporary directory
/// and all its contents when the `TempHome` struct is dropped.
pub fn with_temp_home_dir<F>(f: F)
where
    F: FnOnce(),
{
    let _temp_home = TempHome::new();
    f();
    // TempHome is dropped here, which automatically cleans up the temporary
    // directory
}
