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
// When running tests in parallel, multiple test cases that modify the `HOME`
// environment variable can interfere with each other, causing race conditions
// and intermittent test failures.
//
// The issue occurs because:
// 1. Environment variables are global to the entire process
// 2. Multiple tests running concurrently can overwrite each other's `HOME`
//    settings
// 3. The cleanup code in `Drop` implementations may restore incorrect values
//
// # Solution
//
// A thread-safe solution using a global mutex to serialize access to the `HOME`
// environment variable.

// Use a global mutex to serialize access to HOME environment variable across
// all tests
static HOME_MUTEX: Mutex<()> = Mutex::new(());

/// Helper struct to manage temporary home directory with thread-safe access.
///
/// This struct ensures that when multiple tests run in parallel, they won't
/// interfere with each other's HOME environment variable settings.
/// The mutex ensures exclusive access to the HOME environment variable.
pub struct TempHome {
    _temp_dir: TempDir,
    original_home: Option<String>,
    _guard: std::sync::MutexGuard<'static, ()>,
}

impl TempHome {
    /// Create a new temporary home directory and set it as the HOME environment
    /// variable.
    ///
    /// This will acquire a global mutex to ensure thread-safe access across
    /// parallel tests.
    pub fn new() -> Self {
        // Acquire the lock to ensure exclusive access to HOME environment
        // variable
        let guard = HOME_MUTEX.lock().expect("Failed to lock HOME mutex");

        let temp_dir = TempDir::new().expect("Failed to create temp directory");
        let original_home = env::var("HOME").ok();

        env::set_var("HOME", temp_dir.path());

        Self { _temp_dir: temp_dir, original_home, _guard: guard }
    }

    /// Get the path to the temporary home directory
    #[allow(dead_code)]
    pub fn path(&self) -> &std::path::Path {
        self._temp_dir.path()
    }
}

impl Drop for TempHome {
    fn drop(&mut self) {
        // Restore original HOME - the mutex guard will be released
        // automatically when this struct is dropped, ensuring exclusive
        // access during cleanup
        if let Some(ref home) = self.original_home {
            env::set_var("HOME", home);
        } else {
            env::remove_var("HOME");
        }
        // _guard is dropped here, releasing the mutex
    }
}

/// Execute a closure with a temporary home directory set as HOME environment
/// variable.
///
/// This function provides thread-safe access to HOME environment variable
/// modification for testing purposes.
pub fn with_temp_home_dir<F>(f: F)
where
    F: FnOnce(),
{
    let _temp_home = TempHome::new();
    f();
    // TempHome is dropped here, restoring the original HOME value
}
