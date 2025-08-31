//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::{fs::OpenOptions, io::Write, time::Duration};

    use anyhow::Result;
    use tempfile::NamedTempFile;
    use ten_manager::fs::log_file_watcher::{watch_log_file, LogFileWatchOptions};
    use tokio::{runtime::Runtime, time::sleep};

    use crate::test_case::common::fs::sync_to_disk;

    // Use standard #[test] with manual runtime creation.
    #[test]
    fn test_file_watcher_basic() -> Result<()> {
        let rt = Runtime::new()?;
        rt.block_on(async {
            // Create a temporary file for testing
            let mut temp_file = NamedTempFile::new()?;
            let test_content = "Hello, World!";
            temp_file.write_all(test_content.as_bytes())?;
            temp_file.flush()?;
            sync_to_disk(temp_file.as_file())?;

            // Create options with shorter timeout for testing.
            let options = LogFileWatchOptions {
                timeout: Duration::from_secs(5),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file.
            let mut stream = watch_log_file(temp_file.path(), Some(options)).await?;

            // Get the first chunk.
            let chunk = stream.next().await.expect("Should receive data")?;
            println!("chunk 1: {chunk:?}");
            assert_eq!(chunk.line, test_content);

            // Write more content to the file.
            let more_content = "More content!";
            temp_file.write_all(more_content.as_bytes())?;
            temp_file.flush()?;
            sync_to_disk(temp_file.as_file())?;
            println!("Added more content and synced to disk");

            // Get the second chunk.
            let chunk = stream.next().await;
            println!("chunk 2: {chunk:?}");
            match chunk {
                Some(chunk) => match chunk {
                    Ok(chunk) => assert_eq!(chunk.line, more_content),
                    Err(e) => panic!("Should receive more data: {e}"),
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
                let mut file =
                    OpenOptions::new().write(true).create(true).truncate(true).open(&path_str)?;
                file.write_all("Initial content".as_bytes())?;
                file.flush()?;
                sync_to_disk(&file)?;
            }

            // Create options with shorter timeout for testing
            let options = LogFileWatchOptions {
                timeout: Duration::from_secs(5),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file
            let mut stream = watch_log_file(&path_str, Some(options)).await?;

            // Get the first chunk
            let chunk = stream.next().await.expect("Should receive data")?;
            println!("chunk 1: {chunk:?}");
            assert_eq!(chunk.line, "Initial content");

            // Simulate log rotation - delete and recreate file
            std::fs::remove_file(&path_str)?;
            sleep(Duration::from_millis(200)).await; // Wait a bit for the watcher to detect change

            // Create new file with same name but different content (simulating
            // rotation)
            {
                let mut file =
                    OpenOptions::new().write(true).create(true).truncate(true).open(&path_str)?;
                file.write_all("Rotated content".as_bytes())?;
                file.flush()?;
                sync_to_disk(&file)?;
                println!("Created rotated file and synced to disk");
            }

            // Get the content after rotation
            let chunk = stream.next().await.expect("Should receive rotated data")?;
            println!("chunk 2: {chunk:?}");
            assert_eq!(chunk.line, "Rotated content");

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
            temp_file.write_all("Test content".as_bytes())?;
            temp_file.flush()?;
            sync_to_disk(temp_file.as_file())?;

            // Create options with very short timeout for testing.
            let options = LogFileWatchOptions {
                timeout: Duration::from_millis(500),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file.
            let mut stream = watch_log_file(temp_file.path(), Some(options)).await?;

            // Get the first chunk.
            let chunk = stream.next().await.expect("Should receive data")?;
            println!("chunk 1: {chunk:?}");
            assert_eq!(chunk.line, "Test content");

            // Wait for the timeout to occur (no new content being written).
            let next_result = stream.next().await;
            println!("next_result: {next_result:?}");
            assert!(next_result.is_none(), "Stream should end after timeout");

            Ok(())
        })
    }

    #[test]
    fn test_file_watcher_extension_metadata() -> Result<()> {
        let rt = Runtime::new()?;
        rt.block_on(async {
            // Create a temporary file for testing
            let mut temp_file = NamedTempFile::new()?;

            // Create options with shorter timeout for testing
            let options = LogFileWatchOptions {
                timeout: Duration::from_secs(5),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file (with empty initial content)
            let mut stream = watch_log_file(temp_file.path(), Some(options)).await?;

            // Write the graph resources first (to establish connection between
            // thread ID and extension)
            let graph_resources_with_thread =
                "05-02 22:23:37.397 1713000(1713045) M \
                 ten_extension_thread_log_graph_resources@extension_thread.c:556 [graph \
                 resources] {\"app_base_dir\": \"xxx\", \"app_uri\": \
                 \"msgpack://127.0.0.1:8001/\", \"graph_name\": \"\", \"graph_id\": \
                 \"38097178-1712-4562-b60d-8e6ab15ba0cf\", \"extension_threads\": {\"1713045\": \
                 {\"extensions\": [\"test_extension\"]}}}\n";

            temp_file.write_all(graph_resources_with_thread.as_bytes())?;
            temp_file.flush()?;
            sync_to_disk(temp_file.as_file())?;
            println!("Wrote graph resources and synced to disk");

            // Get the graph resources line
            let chunk = stream.next().await.expect("Should receive graph resources")?;
            println!("chunk 1: {chunk:?}");
            assert!(chunk.line.contains("[graph resources]"));

            // Now write logs that contain extension metadata in format
            // [test_extension]
            let extension_logs =
                "05-02 22:23:37.514 1713000(1713045) I ten_extension_on_start@extension.c:697 \
                 [test_extension] on_start()\n05-02 22:23:37.514 1713000(1713045) I \
                 ten_extension_on_start_done@on_xxx.c:306 [test_extension] on_start() done";

            temp_file.write_all(extension_logs.as_bytes())?;
            temp_file.write_all(b"\n")?;
            temp_file.flush()?;
            sync_to_disk(temp_file.as_file())?;
            println!("Wrote extension log and synced to disk");

            // Get first log with extension metadata
            let chunk = stream.next().await.expect("Should receive log with extension")?;
            println!("chunk 2: {chunk:?}");
            assert!(chunk.line.contains("on_start()"));

            // The metadata should include the extension name
            assert!(chunk.metadata.is_some(), "Should have metadata");
            let metadata = chunk.metadata.unwrap();
            println!("metadata 2: {metadata:?}");
            assert_eq!(metadata.extension, Some("test_extension".to_string()));

            // Get second log with extension metadata
            let chunk = stream.next().await.expect("Should receive log with extension")?;
            println!("chunk 3: {chunk:?}");
            assert!(chunk.line.contains("on_start() done"));

            // The metadata should include the extension name
            assert!(chunk.metadata.is_some(), "Should have metadata");
            let metadata = chunk.metadata.unwrap();
            println!("metadata 3: {metadata:?}");
            assert_eq!(metadata.extension, Some("test_extension".to_string()));

            // Stop watching
            stream.stop();

            Ok(())
        })
    }

    #[test]
    fn test_file_watcher_complete_log_example() -> Result<()> {
        let rt = Runtime::new()?;
        rt.block_on(async {
            // Create a temporary file for testing
            let mut temp_file = NamedTempFile::new()?;

            // Create options with shorter timeout for testing
            let options = LogFileWatchOptions {
                timeout: Duration::from_secs(5),
                buffer_size: 1024,
                check_interval: Duration::from_millis(100),
            };

            // Start watching the file (with empty initial content)
            let mut stream = watch_log_file(temp_file.path(), Some(options)).await?;

            // Write the complete log content from the example
            let complete_log =
                "05-02 22:23:37.301 1713000(1713002) D \
                 ten_extension_context_create@extension_context.c:62 \
                 [38097178-1712-4562-b60d-8e6ab15ba0cf] Create Extension context
05-02 22:23:37.318 1713000(1713001) D ten_app_create_addon_instance@addon.c:396 Try to find addon \
                 for default_extension_group
05-02 22:23:37.318 1713000(1713001) D ten_app_create_addon_instance@addon.c:429 The addon \
                 default_extension_group is loaded and registered using native addon loader \
                 successfully.
05-02 22:23:37.329 1713000(1713002) M \
                 ten_extension_context_log_graph_resources@extension_context.c:352 [graph \
                 resources] {\"app_base_dir\": \"xxx\", \"app_uri\": \
                 \"msgpack://127.0.0.1:8001/\", \"graph_id\": \
                 \"38097178-1712-4562-b60d-8e6ab15ba0cf\" }
05-02 22:23:37.329 1713000(1713045) W pthread_routine@thread.c:114 Failed to set thread name:
05-02 22:23:37.329 1713000(1713045) D ten_extension_thread_main_actual@extension_thread.c:250 \
                 Extension thread is started
05-02 22:23:37.366 1713000(1713045) D ten_extension_group_load_metadata@metadata.c:24 [] Load \
                 metadata
05-02 22:23:37.366 1713000(1713045) D ten_extension_group_on_init_done@on_xxx.c:78 [] on_init() \
                 done
05-02 22:23:37.384 1713000(1713045) I ten_set_default_manifest_info@default.c:25 Skip the loading \
                 of manifest.json because the base_dir of  is missing.
05-02 22:23:37.384 1713000(1713045) I ten_set_default_property_info@default.c:51 Skip the loading \
                 of property.json because the base_dir of  is missing.
05-02 22:23:37.384 1713000(1713045) D ten_extension_group_create_extensions@extension_group.c:174 \
                 [] create_extensions
05-02 22:23:37.386 1713000(1713001) D ten_app_create_addon_instance@addon.c:396 Try to find addon \
                 for basic_empty_extension_group__extension
05-02 22:23:37.387 1713000(1713001) D ten_app_create_addon_instance@addon.c:429 The addon \
                 basic_empty_extension_group__extension is loaded and registered using native \
                 addon loader successfully.
05-02 22:23:37.396 1713000(1713045) I on_addon_create_extension_done@builtin_extension_group.c:92 \
                 Success to create extension test_extension
05-02 22:23:37.396 1713000(1713045) D ten_extension_group_on_create_extensions_done@on_xxx.c:160 \
                 [] create_extensions() done
05-02 22:23:37.397 1713000(1713045) M \
                 ten_extension_thread_log_graph_resources@extension_thread.c:556 [graph \
                 resources] {\"app_base_dir\": \"xxx\", \"app_uri\": \
                 \"msgpack://127.0.0.1:8001/\", \"graph_id\": \
                 \"38097178-1712-4562-b60d-8e6ab15ba0cf\", \"extension_threads\": {\"1713045\": \
                 {\"extensions\": [\"test_extension\"]}}}
05-02 22:23:37.406 1713000(1713002) D \
                 ten_engine_check_if_all_extension_threads_are_ready@extension_interface.c:173 \
                 [msgpack://127.0.0.1:8001/] Engine is ready to handle messages
05-02 22:23:37.460 1713000(1713045) D ten_extension_load_metadata@extension.c:888 [test_extension] \
                 Load metadata
05-02 22:23:37.460 1713000(1713045) D ten_extension_on_configure@extension.c:628 [test_extension] \
                 on_configure()
05-02 22:23:37.460 1713000(1713045) D ten_extension_on_configure_done@on_xxx.c:95 [test_extension] \
                 on_configure() done
05-02 22:23:37.460 1713000(1713045) I ten_extension_handle_ten_namespace_properties@metadata.c:314 \
                 [test_extension] `ten` section is not found in the property, skip
05-02 22:23:37.471 1713000(1713002) D ten_engine_handle_in_msgs_task@common.c:223 \
                 [38097178-1712-4562-b60d-8e6ab15ba0cf] Handle incoming messages
05-02 22:23:37.479 1713000(1713045) D ten_extension_on_init@extension.c:665 [test_extension] \
                 on_init()
05-02 22:23:37.479 1713000(1713045) D ten_extension_on_init_done@on_xxx.c:247 [test_extension] \
                 on_init() done
05-02 22:23:37.506 1713000(1713002) D ten_engine_handle_in_msgs_task@common.c:223 \
                 [38097178-1712-4562-b60d-8e6ab15ba0cf] Handle incoming messages
05-02 22:23:37.514 1713000(1713045) I ten_extension_on_start@extension.c:697 [test_extension] \
                 on_start()
05-02 22:23:37.514 1713000(1713045) I ten_extension_on_start_done@on_xxx.c:306 [test_extension] \
                 on_start() done
05-02 22:23:37.528 1713000(1713000) D ten_test_tcp_client_dump_socket_info@tcp.c:75 Close tcp \
                 client: 127.0.0.1:38742
05-02 22:23:37.528 1713000(1713002) D ten_stream_on_data@protocol_integrated.c:135 Failed to \
                 receive data, close the protocol: -4095
05-02 22:23:37.535 1713000(1713002) D ten_protocol_close@close.c:127 \
                 [2b11c6a8-9a63-4e9e-b18a-92456dee49d5] Try to close communication protocol
05-02 22:23:37.535 1713000(1713002) D ten_protocol_integrated_on_close@close.c:107 \
                 [2b11c6a8-9a63-4e9e-b18a-92456dee49d5] Integrated protocol can be closed now\n";

            temp_file.write_all(complete_log.as_bytes())?;
            temp_file.flush()?;
            sync_to_disk(temp_file.as_file())?;
            println!("Wrote complete log and synced to disk");

            // We need to iterate through all log lines to find the specific
            // ones we're looking for
            let mut target_lines_found = 0;
            let target_start_line = "05-02 22:23:37.514 1713000(1713045) I \
                                     ten_extension_on_start@extension.c:697 [test_extension] \
                                     on_start()";
            let target_done_line = "05-02 22:23:37.514 1713000(1713045) I \
                                    ten_extension_on_start_done@on_xxx.c:306 [test_extension] \
                                    on_start() done";

            // Process all log lines
            while let Some(result) = stream.next().await {
                let chunk = result?;
                println!("chunk: {chunk:?}");

                // Check if this is one of our target lines
                if chunk.line.contains(target_start_line) || chunk.line.contains(target_done_line) {
                    target_lines_found += 1;

                    // Verify that the metadata includes the extension name
                    assert!(chunk.metadata.is_some(), "Should have metadata");
                    let metadata = chunk.metadata.unwrap();
                    println!("metadata: {metadata:?}");
                    assert_eq!(metadata.extension, Some("test_extension".to_string()));
                }

                // If we found both target lines, we can break
                if target_lines_found == 2 {
                    break;
                }
            }

            // Verify that we found both target lines
            assert_eq!(target_lines_found, 2, "Should have found both target lines");

            // Stop watching
            stream.stop();

            Ok(())
        })
    }
}
