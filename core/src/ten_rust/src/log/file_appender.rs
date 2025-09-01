//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fs::{File, OpenOptions},
    io::{self, Write},
    path::{Path, PathBuf},
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, RwLock,
    },
};

// Registry for all reloadable file appenders so we can trigger reopen globally
use tracing_subscriber::fmt::MakeWriter as TracingMakeWriter;

struct Inner {
    path: PathBuf,
    file: RwLock<File>,
    reload: AtomicBool,
}

impl Inner {
    fn reopen_locked(&self) -> io::Result<()> {
        // Ensure parent dir exists if any
        if let Some(parent) = self.path.parent() {
            if !parent.as_os_str().is_empty() {
                std::fs::create_dir_all(parent).ok();
            }
        }

        let file = OpenOptions::new().create(true).append(true).open(&self.path)?;
        let mut guard = self.file.write().unwrap();
        *guard = file;
        Ok(())
    }
}

/// A MakeWriter that writes to a file and can be reopened on demand.
///
/// On every write, it checks an AtomicBool flag. If a reload is requested,
/// one writer will perform a CAS to clear the flag and reopen the file handle.
#[derive(Clone)]
pub struct ReloadableFileAppender {
    inner: Arc<Inner>,
}

impl ReloadableFileAppender {
    /// Create a new reloadable file appender for the given path.
    /// The file is opened lazily on first write or when a reload is requested.
    pub fn new<P: AsRef<Path>>(path: P) -> Self {
        // Open file once at construction to avoid Option<File>
        let path_buf = path.as_ref().to_path_buf();
        if let Some(parent) = path_buf.parent() {
            if !parent.as_os_str().is_empty() {
                std::fs::create_dir_all(parent).ok();
            }
        }

        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&path_buf)
            .expect("Failed to open log file at initialization");

        let inner = Arc::new(Inner {
            path: path_buf,
            file: RwLock::new(file),
            reload: AtomicBool::new(false), // already opened
        });

        Self {
            inner,
        }
    }

    /// Request this appender to reopen the file on next write.
    pub fn request_reopen(&self) {
        self.inner.reload.store(true, Ordering::Release);
    }
}

// Note: We implement MakeWriter by returning self (cloned), so no separate
// Writer struct is needed.

impl Write for ReloadableFileAppender {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        // Fast path: check if reload is needed
        if self.inner.reload.load(Ordering::Relaxed)
            && self
                .inner
                .reload
                .compare_exchange(true, false, Ordering::AcqRel, Ordering::Relaxed)
                .is_ok()
        {
            // This writer is responsible for reopening
            self.inner.reopen_locked()?
        }

        // Perform the actual write under the file lock
        let mut file = self.inner.file.write().unwrap();
        file.write_all(buf)?;
        Ok(buf.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        let mut file = self.inner.file.write().unwrap();
        file.flush()
    }
}

impl<'a> TracingMakeWriter<'a> for ReloadableFileAppender {
    type Writer = ReloadableFileAppender;

    fn make_writer(&'a self) -> Self::Writer {
        self.clone()
    }
}

/// Guard that keeps both the background worker guard and the appender handle
/// alive. This allows requesting file reopen later without any global registry.
pub struct FileAppenderGuard {
    pub non_blocking_guard: tracing_appender::non_blocking::WorkerGuard,
    pub appender: ReloadableFileAppender,
}

impl FileAppenderGuard {
    pub fn request_reopen(&self) {
        self.appender.request_reopen();
    }
}
