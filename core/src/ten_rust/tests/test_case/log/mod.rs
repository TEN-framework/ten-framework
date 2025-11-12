//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::{fs, os::raw::c_char, thread, time::Duration};

    use serial_test::serial;
    use ten_rust::{
        bindings::ten_rust_free_cstring,
        log::{
            bindings::ten_rust_create_log_config_from_json,
            decrypt::decrypt_records_bytes,
            encryption::{AesCtrParams, EncryptionConfig, EncryptionParams},
            reloadable::ten_configure_log_reloadable,
            ten_log, AdvancedLogConfig, AdvancedLogEmitter, AdvancedLogFormatter,
            AdvancedLogHandler, AdvancedLogLevelFilter, AdvancedLogMatcher, ConsoleEmitterConfig,
            FileEmitterConfig, FormatterType, LogLevel, StreamType,
        },
    };
    use tracing::{debug, info, trace};

    fn read_with_backoff(path: &str, max_retries: u32) -> Result<String, std::io::Error> {
        let mut retry_count = 0;

        while retry_count < max_retries {
            match fs::read_to_string(path) {
                Ok(content) if !content.is_empty() => return Ok(content),
                Ok(_) => {
                    thread::sleep(Duration::from_millis(100 * (retry_count + 1) as u64));
                    retry_count += 1;
                    continue;
                }
                Err(e) => {
                    if e.kind() == std::io::ErrorKind::NotFound {
                        thread::sleep(Duration::from_millis(100 * (retry_count + 1) as u64));
                        retry_count += 1;
                        continue;
                    }
                    return Err(e);
                }
            }
        }

        fs::read_to_string(path)
    }

    #[test]
    #[serial]
    fn test_create_log_config_from_json() {
        let log_config_json = r#"
        {
          "handlers": [{
            "matchers": [{
              "level": "debug"
            }],
            "formatter": {
              "type": "plain",
              "colored": false
            },
            "emitter": {
              "type": "console",
              "config": {
                "stream": "stdout"
              }
            }
          }]
        }"#;

        let mut err_msg: *mut c_char = std::ptr::null_mut();

        let log_config_ptr = unsafe {
            let c_string = std::ffi::CString::new(log_config_json).unwrap();
            ten_rust_create_log_config_from_json(c_string.as_ptr(), &mut err_msg)
        };

        if !err_msg.is_null() {
            unsafe {
                let error_string = std::ffi::CStr::from_ptr(err_msg).to_string_lossy();
                println!("Error message: {error_string}");

                ten_rust_free_cstring(err_msg);
            }
            panic!("Function returned error");
        }

        assert!(!log_config_ptr.is_null());

        let log_config = unsafe { Box::from_raw(log_config_ptr as *mut AdvancedLogConfig) };

        assert_eq!(log_config.handlers.len(), 1);
        assert_eq!(log_config.handlers[0].matchers.len(), 1);
        assert_eq!(log_config.handlers[0].matchers[0].level, AdvancedLogLevelFilter::Debug);
        assert_eq!(log_config.handlers[0].formatter.formatter_type, FormatterType::Plain);
        assert_eq!(log_config.handlers[0].formatter.colored, Some(false));
        assert_eq!(
            log_config.handlers[0].emitter,
            AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
                encryption: None,
            })
        );
    }

    #[test]
    #[serial]
    fn test_log_level_info() {
        let temp_file = tempfile::NamedTempFile::new().unwrap();
        let path = temp_file.path().to_str().unwrap();

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: path.to_string(),
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Debug,
            "test_func",
            "test.rs",
            100,
            "Trace message",
        );
        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Debug,
            "test_func",
            "test.rs",
            101,
            "Debug message",
        );
        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Info,
            "test_func",
            "test.rs",
            102,
            "Info message",
        );
        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Warn,
            "test_func",
            "test.rs",
            103,
            "Warn message",
        );
        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Error,
            "test_func",
            "test.rs",
            104,
            "Error message",
        );

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Read log file content with backoff strategy
        let content = read_with_backoff(path, 0).expect("Failed to read log file after retries");

        println!("Log file content:\n{content}");

        // Verify log levels
        assert!(!content.contains("Trace message"), "Trace log should not appear");
        assert!(!content.contains("Debug message"), "Debug log should not appear");
        assert!(content.contains("Info message"), "Info log should appear");
        assert!(content.contains("Warn message"), "Warn log should appear");
        assert!(content.contains("Error message"), "Error log should appear");
    }

    #[test]
    #[serial]
    fn test_formatter_plain_colored() {
        let plain_colored_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Debug, /* Allow all log
                                                       * levels */
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&plain_colored_config).unwrap();
        // Test different log levels to see different colors
        ten_log(
            &plain_colored_config,
            "test_category",
            1234,
            5678,
            LogLevel::Error,
            "test_plain_colored",
            "formatter.rs",
            50,
            "Error message in red",
        );

        ten_log(
            &plain_colored_config,
            "test_category",
            1234,
            5678,
            LogLevel::Warn,
            "test_plain_colored",
            "formatter.rs",
            51,
            "Warning message in yellow",
        );

        ten_log(
            &plain_colored_config,
            "test_category",
            1234,
            5678,
            LogLevel::Info,
            "test_plain_colored",
            "formatter.rs",
            52,
            "Info message in default color",
        );

        ten_log(
            &plain_colored_config,
            "test_category",
            1234,
            5678,
            LogLevel::Debug,
            "test_plain_colored",
            "formatter.rs",
            53,
            "Debug message in blue",
        );
    }

    #[test]
    #[serial]
    fn test_formatter_plain_no_color() {
        let plain_no_color_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&plain_no_color_config).unwrap();

        ten_log(
            &plain_no_color_config,
            "test_category",
            1234,
            5678,
            LogLevel::Error,
            "test_plain_no_color",
            "formatter.rs",
            51,
            "Plain no color message",
        );
    }

    #[test]
    #[serial]
    fn test_formatter_json_no_color() {
        let json_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Json,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&json_config).unwrap();
        ten_log(
            &json_config,
            "test_category",
            1234,
            5678,
            LogLevel::Info,
            "test_json",
            "formatter.rs",
            52,
            "JSON formatted message",
        );
    }

    #[test]
    #[serial]
    fn test_formatter_json_colored() {
        let json_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Debug,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Json,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&json_config).unwrap();
        ten_log(
            &json_config,
            "test_category",
            1234,
            5678,
            LogLevel::Debug,
            "test_json_colored",
            "formatter.rs",
            53,
            "JSON colored message",
        );

        ten_log(
            &json_config,
            "test_category",
            1234,
            5678,
            LogLevel::Error,
            "test_json_colored",
            "formatter.rs",
            54,
            "JSON colored message",
        );
    }

    #[test]
    #[serial]
    fn test_console_emitter_stdout() {
        let stdout_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&stdout_config).unwrap();
        ten_log(
            &stdout_config,
            "test_category",
            1234,
            5678,
            LogLevel::Info,
            "test_stdout",
            "emitter.rs",
            60,
            "Message to stdout",
        );
    }

    #[test]
    #[serial]
    fn test_console_emitter_stderr() {
        let stderr_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Warn,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stderr,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&stderr_config).unwrap();
        ten_log(
            &stderr_config,
            "test_category",
            1234,
            5678,
            LogLevel::Warn,
            "test_stderr",
            "emitter.rs",
            61,
            "Warning message to stderr",
        );
    }

    #[test]
    #[serial]
    fn test_file_emitter_plain() {
        let temp_file = tempfile::NamedTempFile::new().unwrap();
        let test_file = temp_file.path().to_str().unwrap();

        let file_plain_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: test_file.to_string(),
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&file_plain_config).unwrap();
        ten_log(
            &file_plain_config,
            "test_category",
            1234,
            5678,
            LogLevel::Info,
            "test_file_plain",
            "file_emitter.rs",
            70,
            "Plain message to file",
        );
        ten_log(
            &file_plain_config,
            "test_category",
            1234,
            5678,
            LogLevel::Warn,
            "test_file_plain",
            "file_emitter.rs",
            71,
            "Warning message to file",
        );

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let content =
            read_with_backoff(test_file, 0).expect("Failed to read log file after retries");
        println!("File content:\n{content}");

        assert!(content.contains("Plain message to file"), "File should contain log content");
        assert!(content.contains("Warning message to file"), "File should contain warning log");
    }

    #[test]
    #[serial]
    fn test_file_emitter_json() {
        let temp_file = tempfile::NamedTempFile::new().unwrap();
        let test_file = temp_file.path().to_str().unwrap();

        let file_json_config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Debug,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Json,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: test_file.to_string(),
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&file_json_config).unwrap();
        ten_log(
            &file_json_config,
            "test_category",
            1234,
            5678,
            LogLevel::Debug,
            "test_file_json",
            "file_emitter.rs",
            80,
            "JSON message to file",
        );

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let json_content =
            read_with_backoff(test_file, 0).expect("Failed to read log file after retries");
        println!("JSON file content:\n{json_content}");

        assert!(
            json_content.contains("JSON message to file"),
            "JSON file should contain log content"
        );
        println!("JSON file content:\n{json_content}");

        let _ = fs::remove_file(test_file);
    }

    #[test]
    #[serial]
    fn test_file_reopen_after_rename() {
        use tempfile::tempdir;

        // Ensure clean state
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let dir = tempdir().expect("create temp dir");
        let original_path = dir.path().join("reopen_test.log");
        let rotated_path = dir.path().join("reopen_test.log.rotated");
        let original_path_str = original_path.to_str().unwrap().to_string();

        let mut config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: original_path_str.clone(),
                encryption: None,
            }),
        }]);

        // Init reloadable logging
        ten_configure_log_reloadable(&config).unwrap();

        // Write a few lines before rename
        ten_log(
            &config,
            "test_reopen",
            1,
            1,
            LogLevel::Info,
            "before_fn",
            "before.rs",
            1,
            "before-1",
        );
        ten_log(
            &config,
            "test_reopen",
            1,
            1,
            LogLevel::Warn,
            "before_fn",
            "before.rs",
            2,
            "before-2",
        );

        // Give the background worker a brief moment
        thread::sleep(Duration::from_millis(50));

        // Rotate (rename) the current file; writer still holds FD to rotated
        // file
        std::fs::rename(&original_path, &rotated_path).expect("rename log file");

        // Trigger reopen so that subsequent logs go to the original path again
        ten_rust::log::ten_log_reopen_all(&mut config, true);

        // Write more lines after reopen request
        ten_log(&config, "test_reopen", 1, 1, LogLevel::Info, "after_fn", "after.rs", 3, "after-1");
        ten_log(&config, "test_reopen", 1, 1, LogLevel::Warn, "after_fn", "after.rs", 4, "after-2");

        // Force flush: disable all handlers to drop worker guard(s)
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Validate: "before-*" in rotated file only
        let rotated_content =
            read_with_backoff(rotated_path.to_str().unwrap(), 0).expect("read rotated file");
        println!("Rotated content:\n{rotated_content}");
        assert!(rotated_content.contains("before-1"));
        assert!(rotated_content.contains("before-2"));
        assert!(!rotated_content.contains("after-1"));
        assert!(!rotated_content.contains("after-2"));

        // Validate: "after-*" in newly opened original path only
        let new_content =
            read_with_backoff(original_path.to_str().unwrap(), 0).expect("read new log file");
        println!("New content:\n{new_content}");
        assert!(new_content.contains("after-1"));
        assert!(new_content.contains("after-2"));
        assert!(!new_content.contains("before-1"));
        assert!(!new_content.contains("before-2"));
    }

    #[test]
    #[serial]
    fn test_file_emitter_encryption_simple() {
        use tempfile::NamedTempFile;

        // Create temp file for encrypted logs
        let log_file = NamedTempFile::new().expect("Failed to create temp file");
        let log_path = log_file.path().to_str().unwrap().to_string();

        // Build encryption config
        let encryption = EncryptionConfig {
            enabled: Some(true),
            algorithm: Some("AES-CTR".to_string()),
            params: Some(EncryptionParams::AesCtr(AesCtrParams {
                key: "0123456789ABCDEF".to_string(),
                nonce: "FEDCBA9876543210".to_string(),
            })),
        };

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Debug,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: log_path.clone(),
                encryption: Some(encryption.clone()),
            }),
        }]);

        // Initialize logging
        ten_configure_log_reloadable(&config).unwrap();

        // Emit one message
        let msg = "Secret message";
        ten_log(
            &config,
            "test_category",
            1,
            1,
            LogLevel::Info,
            "encrypt_test",
            "encrypt.rs",
            1,
            msg,
        );
        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Warn,
            "encrypt_test",
            "encrypt.rs",
            1,
            "My card number is 1234567890",
        );
        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Debug,
            "encrypt_test",
            "encrypt.rs",
            1,
            "My phone number is 9876543210",
        );

        // Force flush by reloading with empty config
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Read file as bytes once (guard dropped ensures flush)
        let bytes = std::fs::read(&log_path).expect("Failed to read log file bytes");
        assert!(!bytes.is_empty(), "Encrypted log file should not be empty");

        // Decrypt all records via helper module
        let params_json = serde_json::to_string(&AesCtrParams {
            key: "0123456789ABCDEF".to_string(),
            nonce: "FEDCBA9876543210".to_string(),
        })
        .unwrap();
        let decrypted_all = decrypt_records_bytes("AES-CTR", &params_json, &bytes)
            .expect("decrypt_records_bytes should succeed");
        let decrypted_text = String::from_utf8_lossy(&decrypted_all);

        println!("Decrypted content:\n{decrypted_text}");
        assert!(decrypted_text.contains(msg), "Decrypted content should contain original message");
    }

    #[test]
    #[serial]
    fn test_category_matchers_matching_messages() {
        use tempfile::NamedTempFile;

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Create a temporary log file that will be automatically removed when
        // dropped
        let log_file = NamedTempFile::new().expect("Failed to create temp file");

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Info,
                    category: Some("auth".to_string()),
                },
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Debug,
                    category: Some("database".to_string()),
                },
            ],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: log_file.path().to_str().unwrap().to_string(),
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // Messages that should be logged (matching configured rules)
        info!(category = "auth", "Auth service started"); // Matches auth + info
        debug!(category = "auth", "Auth service debug message"); // Won't match any configured rules
        debug!(category = "database", "DB connection pool initialized"); // Matches database + debug
        info!(category = "unknown", "unknown target message"); // Won't match any configured rules

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Read and verify log file contents with backoff strategy
        let log_content = read_with_backoff(log_file.path().to_str().unwrap(), 0)
            .expect("Failed to read log file after retries");

        // Print log content for debugging
        println!("Log file content:\n{log_content}");

        // Verify matching messages are logged
        assert!(log_content.contains("Auth service started"));
        assert!(log_content.contains("DB connection pool initialized"));
        assert!(!log_content.contains("Auth service debug message"));
        assert!(!log_content.contains("unknown target message"));

        // The temp file will be automatically removed when log_file is dropped
    }

    #[test]
    #[serial]
    fn test_category_matchers_non_matching_messages() {
        use tempfile::NamedTempFile;

        // Create a temporary log file that will be automatically removed when
        // dropped
        let log_file = NamedTempFile::new().expect("Failed to create temp file");

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Info,
                    category: Some("auth".to_string()),
                },
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Debug,
                    category: Some("database".to_string()),
                },
            ],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: log_file.path().to_str().unwrap().to_string(),
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // Messages that should not be logged (level mismatch)
        debug!(category = "auth", "Auth debug message"); // Won't match: auth only allows info
        trace!(category = "database", "DB trace message"); // Won't match: database only allows debug

        // Messages that should not be logged (category not configured)
        info!(category = "network", "Network info message");
        debug!(category = "network", "Network debug message");

        // Messages that should not be logged (default category)
        info!("Default category info message");
        debug!("Default category debug message");

        // Read and verify log file contents
        let log_content = fs::read_to_string(log_file.path()).expect("Failed to read log file");

        // Verify non-matching messages are not logged
        assert!(!log_content.contains("Auth debug message"));
        assert!(!log_content.contains("DB trace message"));
        assert!(!log_content.contains("Network"));
        assert!(!log_content.contains("Default category"));

        // The temp file will be automatically removed when log_file is dropped
    }

    #[test]
    #[serial]
    fn test_multiple_handlers_simplified() {
        use tracing::{debug, info, warn};

        // Create two temporary files for different handlers
        let auth_file = tempfile::NamedTempFile::new().unwrap();
        let db_file = tempfile::NamedTempFile::new().unwrap();

        let config = AdvancedLogConfig::new(vec![
            // Handler 1: Auth logs (INFO and above) to auth_file
            AdvancedLogHandler {
                matchers: vec![AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Info,
                    category: Some("auth".to_string()),
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Plain,
                    colored: Some(false),
                },
                emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                    path: auth_file.path().to_str().unwrap().to_string(),
                    encryption: None,
                }),
            },
            // Handler 2: Database logs (all levels) to db_file
            AdvancedLogHandler {
                matchers: vec![AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Debug,
                    category: Some("database".to_string()),
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Plain,
                    colored: Some(false),
                },
                emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                    path: db_file.path().to_str().unwrap().to_string(),
                    encryption: None,
                }),
            },
        ]);

        ten_configure_log_reloadable(&config).unwrap();

        // Auth logs at different levels
        info!(category = "auth", "User login successful"); // Should appear in auth_file
        warn!(category = "auth", "Failed login attempt"); // Should appear in auth_file
        debug!(category = "auth", "Auth token details"); // Should NOT appear in auth_file

        // Database logs at different levels
        info!(category = "database", "Connection established"); // Should appear in db_file
        debug!(category = "database", "Query executed: SELECT * FROM users"); // Should appear in db_file
        debug!(category = "database", "Connection pool stats: 5 active"); // Should appear in db_file

        // Other category logs (should not appear in either file)
        info!(category = "network", "Server started");
        debug!(category = "network", "Socket initialized");

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Read and verify auth file contents with backoff strategy
        let auth_content = read_with_backoff(auth_file.path().to_str().unwrap(), 0)
            .expect("Failed to read auth log file after retries");

        // Verify auth file contents
        assert!(
            auth_content.contains("User login successful"),
            "Auth file should contain info level message"
        );
        assert!(
            auth_content.contains("Failed login attempt"),
            "Auth file should contain warn level message"
        );
        assert!(
            !auth_content.contains("Auth token details"),
            "Auth file should not contain debug level message"
        );
        assert!(!auth_content.contains("database"), "Auth file should not contain database logs");
        assert!(!auth_content.contains("network"), "Auth file should not contain network logs");

        // Read and verify database file contents with backoff strategy
        let db_content = read_with_backoff(db_file.path().to_str().unwrap(), 0)
            .expect("Failed to read database log file after retries");

        println!("DB file content:\n{db_content}");

        // Verify database file contents
        assert!(
            db_content.contains("Connection established"),
            "DB file should contain info level message"
        );
        assert!(
            db_content.contains("Query executed"),
            "DB file should contain debug level message"
        );
        assert!(
            db_content.contains("Connection pool stats"),
            "DB file should contain debug level message"
        );
        assert!(!db_content.contains("auth"), "DB file should not contain auth logs");
        assert!(!db_content.contains("network"), "DB file should not contain network logs");
    }

    #[test]
    #[serial]
    fn test_default_config_no_handlers() {
        let config_no_handlers = AdvancedLogConfig::new(vec![]);

        ten_configure_log_reloadable(&config_no_handlers).unwrap();
        ten_log(
            &config_no_handlers,
            "test_category",
            1234,
            5678,
            LogLevel::Info,
            "test_default",
            "default.rs",
            100,
            "Default config info",
        );
    }

    #[test]
    #[serial]
    fn test_file_reopen_with_frequent_logging() {
        use std::sync::{
            atomic::{AtomicBool, Ordering},
            Arc,
        };

        use tempfile::tempdir;

        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let dir = tempdir().expect("create temp dir");
        let log_path = dir.path().join("frequent_test.log");
        let log_path_str = log_path.to_str().unwrap().to_string();

        let handler = AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: log_path_str.clone(),
                encryption: None,
            }),
        };

        let config = Arc::new(std::sync::Mutex::new(AdvancedLogConfig::new(vec![handler.clone()])));

        ten_configure_log_reloadable(&config.lock().unwrap()).unwrap();

        let should_stop = Arc::new(AtomicBool::new(false));
        let should_stop_clone = should_stop.clone();
        let config_clone = config.clone();

        let logging_thread = thread::spawn(move || {
            let mut counter = 0;
            while !should_stop_clone.load(Ordering::Relaxed) {
                ten_log(
                    &config_clone.lock().unwrap(),
                    "test_frequent",
                    1,
                    1,
                    LogLevel::Info,
                    "test_fn",
                    "test.rs",
                    1,
                    &format!("log message {counter}"),
                );
                counter += 1;
                thread::sleep(Duration::from_micros(100));
            }
            counter
        });

        for _ in 0..5 {
            thread::sleep(Duration::from_millis(100));
            let mut guard = config.lock().unwrap();
            ten_rust::log::ten_log_reopen_all(&mut guard, true);
        }

        should_stop.store(true, Ordering::Relaxed);
        let total_logs = logging_thread.join().unwrap();

        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let content = read_with_backoff(log_path.to_str().unwrap(), 0).expect("read log file");

        println!("Log content:\n{content}");
        println!("Total logs written: {total_logs}");

        let mut found_logs = 0;
        for line in content.lines() {
            if line.contains("log message") {
                found_logs += 1;
            }
        }

        assert_eq!(found_logs, total_logs, "Some logs were lost during the reopen process");
    }

    #[test]
    #[serial]
    fn test_file_reopen_with_rename_and_frequent_logging() {
        use std::sync::{
            atomic::{AtomicBool, Ordering},
            Arc,
        };

        use tempfile::tempdir;

        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let dir = tempdir().expect("create temp dir");
        let original_path = dir.path().join("rename_test.log");
        let rotated_paths: Vec<String> = (0..3)
            .map(|i| dir.path().join(format!("rename_test.log.{i}")).to_str().unwrap().to_string())
            .collect();
        let original_path_str = original_path.to_str().unwrap().to_string();

        let handler = AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: original_path_str.clone(),
                encryption: None,
            }),
        };

        let config = Arc::new(std::sync::Mutex::new(AdvancedLogConfig::new(vec![handler.clone()])));

        ten_configure_log_reloadable(&config.lock().unwrap()).unwrap();

        let should_stop = Arc::new(AtomicBool::new(false));
        let should_stop_clone = should_stop.clone();
        let config_clone = config.clone();

        let logging_thread = thread::spawn(move || {
            let mut counter = 0;
            while !should_stop_clone.load(Ordering::Relaxed) {
                ten_log(
                    &config_clone.lock().unwrap(),
                    "test_rename",
                    1,
                    1,
                    LogLevel::Info,
                    "test_fn",
                    "test.rs",
                    1,
                    &format!("log message {counter}"),
                );
                counter += 1;
                thread::sleep(Duration::from_micros(100));
            }
            counter
        });

        for rotated_path in &rotated_paths {
            thread::sleep(Duration::from_millis(100));

            std::fs::rename(&original_path, rotated_path).expect("rename log file");

            let mut guard = config.lock().unwrap();
            ten_rust::log::ten_log_reopen_all(&mut guard, true);

            thread::sleep(Duration::from_millis(50));
        }

        should_stop.store(true, Ordering::Relaxed);
        let total_logs = logging_thread.join().unwrap();

        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let mut all_content = String::new();

        for rotated_path in &rotated_paths {
            let content = read_with_backoff(rotated_path, 0).expect("read rotated log file");
            all_content.push_str(&content);
        }

        let final_content =
            read_with_backoff(original_path.to_str().unwrap(), 0).expect("read current log file");
        all_content.push_str(&final_content);

        println!("Combined log content:\n{all_content}");
        println!("Total logs written: {total_logs}");

        let mut found_logs = 0;
        for line in all_content.lines() {
            if line.contains("log message") {
                found_logs += 1;
            }
        }

        assert_eq!(
            found_logs, total_logs,
            "Some logs were lost during the rename and reopen process"
        );
    }

    #[test]
    #[serial]
    fn test_actual_logging_output() {
        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::Debug,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Json,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        ten_log(
            &config,
            "test_category",
            9999,
            8888,
            LogLevel::Debug,
            "main",
            "app.rs",
            10,
            "Application started",
        );
        ten_log(
            &config,
            "test_category",
            9999,
            8888,
            LogLevel::Info,
            "auth",
            "auth.rs",
            25,
            "User login successful",
        );
        ten_log(
            &config,
            "test_category",
            9999,
            8888,
            LogLevel::Warn,
            "db",
            "database.rs",
            50,
            "Database connection pool almost full",
        );
        ten_log(
            &config,
            "test_category",
            9999,
            8888,
            LogLevel::Error,
            "network",
            "network.rs",
            75,
            "Network connection timeout",
        );

        ten_log(
            &config,
            "test_category",
            9999,
            8888,
            LogLevel::Debug,
            "parser",
            "json_parser.rs",
            100,
            "Parse JSON: {\"key\": \"value\"}",
        );
    }

    #[test]
    #[serial]
    fn test_log_level_off_global() {
        use tempfile::NamedTempFile;

        // Create a pre-existing empty file to ensure the path exists even if no
        // logs are written
        let temp_file = NamedTempFile::new().unwrap();
        let path = temp_file.path().to_str().unwrap();

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::OFF,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: path.to_string(),
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // These should all be dropped due to global OFF
        ten_log(&config, "any", 1, 1, LogLevel::Info, "f", "f.rs", 1, "global-off-info");
        ten_log(&config, "any", 1, 1, LogLevel::Warn, "f", "f.rs", 2, "global-off-warn");
        ten_log(&config, "any", 1, 1, LogLevel::Error, "f", "f.rs", 3, "global-off-error");

        // Force flush logs (drop workers)
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Verify file remains empty
        let content = read_with_backoff(path, 0).expect("read log file");
        assert!(content.is_empty(), "Global OFF should produce no output");
    }

    #[test]
    #[serial]
    fn test_log_level_off_specific_category() {
        use tempfile::NamedTempFile;

        // Create a pre-existing empty file to ensure the path exists even if no
        // logs are written
        let temp_file = NamedTempFile::new().unwrap();
        let path = temp_file.path().to_str().unwrap();

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevelFilter::OFF,
                category: Some("auth".to_string()),
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: path.to_string(),
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // These should all be dropped due to global OFF
        ten_log(&config, "auth", 1, 1, LogLevel::Info, "f", "f.rs", 1, "aaa");
        ten_log(&config, "auth", 1, 1, LogLevel::Warn, "f", "f.rs", 2, "bbb");
        ten_log(&config, "database", 1, 1, LogLevel::Error, "f", "f.rs", 3, "ccc");
        ten_log(&config, "", 1, 1, LogLevel::Error, "f", "f.rs", 3, "ddd");

        // Force flush logs (drop workers)
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Verify file remains empty
        let content = read_with_backoff(path, 0).expect("read log file");
        assert!(content.is_empty(), "Global OFF should produce no output");
    }

    #[test]
    #[serial]
    fn test_category_off_overrides_global_debug() {
        use tempfile::NamedTempFile;

        // Ensure clean state
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let log_file = NamedTempFile::new().expect("create temp file");
        let path = log_file.path().to_str().unwrap().to_string();

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![
                // Allow all categories at debug level and above
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Debug,
                    category: None,
                },
                // But turn off a specific category completely
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::OFF,
                    category: Some("auth".to_string()),
                },
            ],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // These should be dropped due to auth=off
        debug!(category = "auth", "auth-debug-should-not-appear");
        info!(category = "auth", "auth-info-should-not-appear");
        // This should pass due to default debug for other categories
        debug!(category = "database", "db-debug-should-appear");
        info!(category = "database", "db-info-should-appear");

        // Force flush
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let content = read_with_backoff(log_file.path().to_str().unwrap(), 0)
            .expect("Failed to read log file after retries");

        assert!(!content.contains("auth-debug-should-not-appear"));
        assert!(!content.contains("auth-info-should-not-appear"));
        assert!(content.contains("db-debug-should-appear"));
        assert!(content.contains("db-info-should-appear"));
    }

    #[test]
    #[serial]
    fn test_global_off_then_debug_should_allow_logs() {
        use tempfile::NamedTempFile;

        // Ensure clean state to avoid prior global config influencing results
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let temp_file = NamedTempFile::new().unwrap();
        let path = temp_file.path().to_str().unwrap().to_string();

        // Two matchers in the same handler (both category=None): OFF first,
        // then DEBUG With equal specificity, the later directive should
        // override the earlier one; effective level is DEBUG
        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::OFF,
                    category: None,
                },
                AdvancedLogMatcher {
                    level: AdvancedLogLevelFilter::Debug,
                    category: None,
                },
            ],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path,
                encryption: None,
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // These should be written (effective level is DEBUG)
        ten_log(&config, "any", 1, 1, LogLevel::Debug, "f", "f.rs", 1, "off-then-debug-debug");
        ten_log(&config, "any", 1, 1, LogLevel::Info, "f", "f.rs", 2, "off-then-debug-info");

        // Force flush
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        let content = read_with_backoff(temp_file.path().to_str().unwrap(), 0)
            .expect("Failed to read log file after retries");

        // Expect log output present
        assert!(content.contains("off-then-debug-debug"));
        assert!(content.contains("off-then-debug-info"));
    }
}
