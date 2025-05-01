//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::fs::OpenOptions;
    use std::io::Write;
    use std::time::Duration;

    use anyhow::Result;
    use tempfile::NamedTempFile;
    use ten_manager::fs::file_watcher::{watch_file, FileWatchOptions};
    use tokio::runtime::Runtime;
    use tokio::time::sleep;

    // Use standard #[test] with manual runtime creation.
    #[test]
    fn test_file_watcher_basic() -> Result<()> {
        let rt = Runtime::new()?;
        rt.block_on(async {
            // Create a temporary file for testing
            let mut temp_file = NamedTempFile::new()?;
            let test_content = b"Hello, World!";
            temp_file.write_all(test_content)?;
            temp_file.flush()?;

            // Create options with shorter timeout for testing.
            let options = FileWatchOptions {
                timeout: Duration::from_secs(5),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file.
            let mut stream =
                watch_file(temp_file.path(), Some(options)).await?;

            // Get the first chunk.
            let chunk = stream.next().await.expect("Should receive data")?;
            assert_eq!(chunk, test_content);

            // Write more content to the file.
            let more_content = b"More content!";
            temp_file.write_all(more_content)?;
            temp_file.flush()?;

            // Get the second chunk.
            let chunk = stream.next().await;
            match chunk {
                Some(chunk) => match chunk {
                    Ok(chunk) => assert_eq!(chunk, more_content),
                    Err(e) => panic!("Should receive more data: {}", e),
                },
                None => {
                    panic!("Should receive more data");
                }
            }

            // Stop watching.
            println!("Stopping stream");
            stream.stop();

            Ok(())
        })
    }

    #[test]
    fn test_file_watcher_rotation() -> Result<()> {
        let rt = Runtime::new()?;
        rt.block_on(async {
            // Create a temporary file for testing
            let temp_path = NamedTempFile::new()?.into_temp_path();
            let path_str = temp_path.to_str().unwrap().to_string();

            // Write initial content
            {
                let mut file = OpenOptions::new()
                    .write(true)
                    .create(true)
                    .truncate(true)
                    .open(&path_str)?;
                file.write_all(b"Initial content")?;
                file.flush()?;
            }

            // Create options with shorter timeout for testing
            let options = FileWatchOptions {
                timeout: Duration::from_secs(5),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file
            let mut stream = watch_file(&path_str, Some(options)).await?;

            // Get the first chunk
            let chunk = stream.next().await.expect("Should receive data")?;
            assert_eq!(chunk, b"Initial content");

            // Simulate log rotation - delete and recreate file
            std::fs::remove_file(&path_str)?;
            sleep(Duration::from_millis(200)).await; // Wait a bit for the watcher to detect change

            // Create new file with same name but different content (simulating
            // rotation)
            {
                let mut file = OpenOptions::new()
                    .write(true)
                    .create(true)
                    .truncate(true)
                    .open(&path_str)?;
                file.write_all(b"Rotated content")?;
                file.flush()?;
            }

            // Get the content after rotation
            let chunk =
                stream.next().await.expect("Should receive rotated data")?;
            assert_eq!(chunk, b"Rotated content");

            // Stop watching
            stream.stop();

            Ok(())
        })
    }

    #[test]
    fn test_file_watcher_timeout() -> Result<()> {
        let rt = Runtime::new()?;
        rt.block_on(async {
            // Create a temporary file for testing.
            let mut temp_file = NamedTempFile::new()?;
            temp_file.write_all(b"Test content")?;
            temp_file.flush()?;

            // Create options with very short timeout for testing.
            let options = FileWatchOptions {
                timeout: Duration::from_millis(500),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file.
            let mut stream =
                watch_file(temp_file.path(), Some(options)).await?;

            // Get the first chunk.
            let chunk = stream.next().await.expect("Should receive data")?;
            assert_eq!(chunk, b"Test content");

            // Wait for the timeout to occur (no new content being written).
            let next_result = stream.next().await;
            assert!(next_result.is_none(), "Stream should end after timeout");

            Ok(())
        })
    }
}
