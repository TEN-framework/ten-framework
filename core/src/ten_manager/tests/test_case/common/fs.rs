//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::time::Duration;

use anyhow::Result;

// Helper function to ensure content is synced to disk.
pub fn sync_to_disk(file: &std::fs::File) -> Result<()> {
    // Platform-specific sync implementation.
    #[cfg(unix)]
    unsafe {
        use std::os::unix::io::AsRawFd;

        // Call fsync to ensure data is written to disk.
        if libc::fsync(file.as_raw_fd()) != 0 {
            return Err(anyhow::anyhow!("fsync failed"));
        }
    }

    #[cfg(windows)]
    unsafe {
        // Windows equivalent of fsync is FlushFileBuffers.
        use std::os::windows::io::AsRawHandle;

        if winapi::um::fileapi::FlushFileBuffers(file.as_raw_handle() as _) == 0 {
            return Err(anyhow::anyhow!(
                "FlushFileBuffers failed with error code: {}",
                std::io::Error::last_os_error()
            ));
        }
    }

    // Cross-platform flush (less reliable but always available).
    #[cfg(not(any(unix, windows)))]
    {
        // Use standard flush and sync_all for other platforms.
        file.sync_all()?;
    }

    // Give the filesystem a moment to update metadata.
    std::thread::sleep(Duration::from_millis(50));

    Ok(())
}
