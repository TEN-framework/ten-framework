//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fs::{File, Metadata};
use std::io::{self, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use anyhow::{anyhow, Result};
use tokio::sync::mpsc::{self, Receiver, Sender};
use tokio::sync::oneshot;
use tokio::time;

const DEFAULT_TIMEOUT: Duration = Duration::from_secs(60); // 1 minute timeout.
const DEFAULT_BUFFER_SIZE: usize = 4096; // Default read buffer size.
const DEFAULT_CHECK_INTERVAL: Duration = Duration::from_millis(100);

/// Stream of file content changes.
pub struct FileContentStream {
    // Channel for receiving file content.
    content_rx: Receiver<Result<Vec<u8>>>,

    // Sender to signal stop request.
    stop_tx: Option<oneshot::Sender<()>>,
}

impl FileContentStream {
    /// Create a new FileContentStream.
    fn new(
        content_rx: Receiver<Result<Vec<u8>>>,
        stop_tx: oneshot::Sender<()>,
    ) -> Self {
        Self { content_rx, stop_tx: Some(stop_tx) }
    }

    /// Get the next chunk of data from the file.
    pub async fn next(&mut self) -> Option<Result<Vec<u8>>> {
        self.content_rx.recv().await
    }

    /// Stop the file watching process.
    pub fn stop(&mut self) {
        if let Some(tx) = self.stop_tx.take() {
            let _ = tx.send(());
        }
    }
}

impl Drop for FileContentStream {
    fn drop(&mut self) {
        self.stop();
    }
}

/// Options for watching a file.
#[derive(Clone)]
pub struct FileWatchOptions {
    /// Timeout for waiting for new content after reaching EOF.
    pub timeout: Duration,

    /// Size of buffer for reading.
    pub buffer_size: usize,

    /// Interval to check for new content when at EOF.
    pub check_interval: Duration,
}

impl Default for FileWatchOptions {
    fn default() -> Self {
        Self {
            timeout: DEFAULT_TIMEOUT,
            buffer_size: DEFAULT_BUFFER_SIZE,
            check_interval: DEFAULT_CHECK_INTERVAL,
        }
    }
}

/// Watch a file for changes and stream its content.
///
/// Returns a FileContentStream that can be used to read the content of the file
/// as it changes. The stream will end when either:
/// 1. The caller stops it by calling `stop()` or dropping the stream.
/// 2. No new content is available after reaching EOF and the timeout is
///    reached.
pub async fn watch_file<P: AsRef<Path>>(
    path: P,
    options: Option<FileWatchOptions>,
) -> Result<FileContentStream> {
    let path = path.as_ref().to_path_buf();

    // Ensure the file exists before we start watching it.
    if !path.exists() {
        return Err(anyhow!("File does not exist: {}", path.display()));
    }

    let options = options.unwrap_or_default();

    // Create channels.
    let (content_tx, content_rx) = mpsc::channel(32);
    let (stop_tx, stop_rx) = oneshot::channel();

    // Spawn a task to watch the file.
    tokio::spawn(async move {
        watch_file_task(path, content_tx, stop_rx, options).await;
    });

    Ok(FileContentStream::new(content_rx, stop_tx))
}

async fn watch_file_task(
    path: PathBuf,
    content_tx: Sender<Result<Vec<u8>>>,
    mut stop_rx: oneshot::Receiver<()>,
    options: FileWatchOptions,
) {
    let mut last_position: u64 = 0;
    let mut last_metadata: Option<Metadata> = None;
    let mut eof_reached = false;
    let mut eof_time: Option<Instant> = None;

    'outer: loop {
        // Check if we should stop.
        if stop_rx.try_recv().is_ok() {
            break;
        }

        // Try to open the file.
        let file_result = File::open(&path);
        match file_result {
            Ok(mut file) => {
                // Check if the file has been rotated by comparing metadata.
                let metadata = match file.metadata() {
                    Ok(meta) => meta,
                    Err(e) => {
                        if let Err(e) = content_tx
                            .send(Err(anyhow!(
                                "Failed to get file metadata: {}",
                                e
                            )))
                            .await
                        {
                            eprintln!("Failed to send error: {}", e);
                        }
                        break;
                    }
                };

                let file_rotated = match &last_metadata {
                    Some(last_meta) => {
                        !same_file_metadata(last_meta, &metadata)
                    }
                    None => false,
                };

                // Update metadata for next comparison.
                last_metadata = Some(metadata);

                // If the file was rotated, reset position to the beginning.
                if file_rotated {
                    last_position = 0;
                    eof_reached = false;
                    eof_time = None;
                }

                // Seek to the last position.
                if let Err(e) = file.seek(SeekFrom::Start(last_position)) {
                    if let Err(e) = content_tx
                        .send(Err(anyhow!("Failed to seek in file: {}", e)))
                        .await
                    {
                        eprintln!("Failed to send error: {}", e);
                    }
                    break;
                }

                // Read new content.
                let mut buffer = vec![0; options.buffer_size];

                // Continuously read until we reach EOF or error.
                loop {
                    // Check if we should stop.
                    if stop_rx.try_recv().is_ok() {
                        break 'outer;
                    }

                    match file.read(&mut buffer) {
                        Ok(0) => {
                            // EOF reached.
                            if !eof_reached {
                                eof_reached = true;
                                eof_time = Some(Instant::now());
                            }

                            // If we've been at EOF for too long, exit.
                            if let Some(time) = eof_time {
                                if time.elapsed() > options.timeout {
                                    // Send EOF marker and exit.
                                    break 'outer;
                                }
                            }

                            // Wait a bit before checking again.
                            match time::timeout(
                                options.check_interval,
                                &mut stop_rx,
                            )
                            .await
                            {
                                Ok(Ok(())) => break 'outer, /* Stop received during wait */
                                Ok(Err(_)) => {} // Timeout elapsed normally
                                Err(_) => {}     // Timeout elapsed
                            }

                            break; // Break inner loop to reopen the file.
                        }
                        Ok(n) => {
                            // Reset EOF flags since we got new data.
                            eof_reached = false;
                            eof_time = None;

                            // Send the data we read.
                            let data = buffer[..n].to_vec();
                            last_position += n as u64;

                            if let Err(e) = content_tx.send(Ok(data)).await {
                                eprintln!("Failed to send data: {}", e);
                                break 'outer;
                            }
                        }
                        Err(e) => {
                            // Handle other read errors.
                            if e.kind() != io::ErrorKind::Interrupted {
                                if let Err(e) = content_tx
                                    .send(Err(anyhow!(
                                        "Failed to read from file: {}",
                                        e
                                    )))
                                    .await
                                {
                                    eprintln!("Failed to send error: {}", e);
                                }
                                break 'outer;
                            }
                        }
                    }
                }
            }
            Err(e) => {
                // If the file doesn't exist but did before, it might have been
                // deleted during rotation.
                if path.exists() {
                    // It exists but we can't open it for some reason.
                    if let Err(e) = content_tx
                        .send(Err(anyhow!("Failed to open file: {}", e)))
                        .await
                    {
                        eprintln!("Failed to send error: {}", e);
                    }
                    break;
                } else {
                    // Wait a bit and retry, file might reappear after rotation.
                    match time::timeout(options.check_interval, &mut stop_rx)
                        .await
                    {
                        Ok(Ok(())) => break, // Stop received during wait.
                        Ok(Err(_)) => {}     // Timeout elapsed normally.
                        Err(_) => {}         // Timeout elapsed.
                    }
                }
            }
        }
    }
}

// Helper function to compare file metadata.
fn same_file_metadata(a: &Metadata, b: &Metadata) -> bool {
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        a.dev() == b.dev() && a.ino() == b.ino()
    }

    #[cfg(windows)]
    {
        // On Windows, we can't reliably determine if two file handles point to
        // the same file. Best approximation is comparing file sizes and
        // modification times.
        use std::os::windows::fs::MetadataExt;
        a.file_size() == b.file_size()
            && a.last_write_time() == b.last_write_time()
    }

    #[cfg(not(any(unix, windows)))]
    {
        // For other platforms, just compare size and modification time.
        a.len() == b.len() && a.modified().ok() == b.modified().ok()
    }
}
