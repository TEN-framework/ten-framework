//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use serial_test::serial;
    use std::fs;
    use std::os::raw::c_char;
    use std::thread;
    use std::time::Duration;
    use ten_rust::{
        bindings::ten_rust_free_cstring,
        log::{
            bindings::ten_rust_create_log_config_from_json,
            reloadable::ten_configure_log_reloadable, ten_log,
            AdvancedLogConfig, AdvancedLogEmitter, AdvancedLogFormatter,
            AdvancedLogHandler, AdvancedLogLevel, AdvancedLogMatcher,
            ConsoleEmitterConfig, FileEmitterConfig, FormatterType, LogLevel,
            StreamType,
        },
    };
    use tracing::{debug, info, trace};

    fn read_with_backoff(
        path: &str,
        max_retries: u32,
    ) -> Result<String, std::io::Error> {
        let mut retry_count = 0;

        while retry_count < max_retries {
            match fs::read_to_string(path) {
                Ok(content) if !content.is_empty() => return Ok(content),
                Ok(_) => {
                    thread::sleep(Duration::from_millis(
                        100 * (retry_count + 1) as u64,
                    ));
                    retry_count += 1;
                    continue;
                }
                Err(e) => {
                    if e.kind() == std::io::ErrorKind::NotFound {
                        thread::sleep(Duration::from_millis(
                            100 * (retry_count + 1) as u64,
                        ));
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
            ten_rust_create_log_config_from_json(
                c_string.as_ptr(),
                &mut err_msg,
            )
        };

        if !err_msg.is_null() {
            unsafe {
                let error_string =
                    std::ffi::CStr::from_ptr(err_msg).to_string_lossy();
                println!("Error message: {error_string}");

                ten_rust_free_cstring(err_msg);
            }
            panic!("Function returned error");
        }

        assert!(!log_config_ptr.is_null());

        let log_config =
            unsafe { Box::from_raw(log_config_ptr as *mut AdvancedLogConfig) };

        assert_eq!(log_config.handlers.len(), 1);
        assert_eq!(log_config.handlers[0].matchers.len(), 1);
        assert_eq!(
            log_config.handlers[0].matchers[0].level,
            AdvancedLogLevel::Debug
        );
        assert_eq!(
            log_config.handlers[0].formatter.formatter_type,
            FormatterType::Plain
        );
        assert_eq!(log_config.handlers[0].formatter.colored, Some(false));
        assert_eq!(
            log_config.handlers[0].emitter,
            AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
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
                level: AdvancedLogLevel::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: path.to_string(),
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        ten_log(
            &config,
            "test_category",
            1234,
            5678,
            LogLevel::Verbose,
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
        let content = read_with_backoff(path, 0)
            .expect("Failed to read log file after retries");

        println!("Log file content:\n{content}");

        // Verify log levels
        assert!(
            !content.contains("Trace message"),
            "Trace log should not appear"
        );
        assert!(
            !content.contains("Debug message"),
            "Debug log should not appear"
        );
        assert!(content.contains("Info message"), "Info log should appear");
        assert!(content.contains("Warn message"), "Warn log should appear");
        assert!(content.contains("Error message"), "Error log should appear");
    }

    #[test]
    #[serial]
    fn test_formatter_plain_colored() {
        let plain_colored_config =
            AdvancedLogConfig::new(vec![AdvancedLogHandler {
                matchers: vec![AdvancedLogMatcher {
                    level: AdvancedLogLevel::Trace, // Allow all log levels
                    category: None,
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Plain,
                    colored: Some(true),
                },
                emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                    stream: StreamType::Stdout,
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
        let plain_no_color_config =
            AdvancedLogConfig::new(vec![AdvancedLogHandler {
                matchers: vec![AdvancedLogMatcher {
                    level: AdvancedLogLevel::Info,
                    category: None,
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Plain,
                    colored: Some(false),
                },
                emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                    stream: StreamType::Stdout,
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
                level: AdvancedLogLevel::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Json,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
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
                level: AdvancedLogLevel::Debug,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Json,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
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
                level: AdvancedLogLevel::Info,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
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
                level: AdvancedLogLevel::Warn,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stderr,
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

        let file_plain_config =
            AdvancedLogConfig::new(vec![AdvancedLogHandler {
                matchers: vec![AdvancedLogMatcher {
                    level: AdvancedLogLevel::Info,
                    category: None,
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Plain,
                    colored: Some(false),
                },
                emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                    path: test_file.to_string(),
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

        let content = read_with_backoff(test_file, 0)
            .expect("Failed to read log file after retries");
        println!("File content:\n{content}");

        assert!(
            content.contains("Plain message to file"),
            "File should contain log content"
        );
        assert!(
            content.contains("Warning message to file"),
            "File should contain warning log"
        );
    }

    #[test]
    #[serial]
    fn test_file_emitter_json() {
        let temp_file = tempfile::NamedTempFile::new().unwrap();
        let test_file = temp_file.path().to_str().unwrap();

        let file_json_config =
            AdvancedLogConfig::new(vec![AdvancedLogHandler {
                matchers: vec![AdvancedLogMatcher {
                    level: AdvancedLogLevel::Debug,
                    category: None,
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Json,
                    colored: Some(true),
                },
                emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                    path: test_file.to_string(),
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

        let json_content = read_with_backoff(test_file, 0)
            .expect("Failed to read log file after retries");
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
    fn test_category_matchers_matching_messages() {
        use tempfile::NamedTempFile;

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Create a temporary log file that will be automatically removed when
        // dropped
        let log_file =
            NamedTempFile::new().expect("Failed to create temp file");

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![
                AdvancedLogMatcher {
                    level: AdvancedLogLevel::Info,
                    category: Some("auth".to_string()),
                },
                AdvancedLogMatcher {
                    level: AdvancedLogLevel::Debug,
                    category: Some("database".to_string()),
                },
            ],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: log_file.path().to_str().unwrap().to_string(),
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // Messages that should be logged (matching configured rules)
        info!(target: "auth", "Auth service started"); // Matches auth + info
        debug!(target: "auth", "Auth service debug message"); // Won't match any configured rules
        debug!(target: "database", "DB connection pool initialized"); // Matches database + debug
        info!(target: "unknown", "unknown target message"); // Won't match any configured rules

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Read and verify log file contents with backoff strategy
        let log_content =
            read_with_backoff(log_file.path().to_str().unwrap(), 0)
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
        let log_file =
            NamedTempFile::new().expect("Failed to create temp file");

        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![
                AdvancedLogMatcher {
                    level: AdvancedLogLevel::Info,
                    category: Some("auth".to_string()),
                },
                AdvancedLogMatcher {
                    level: AdvancedLogLevel::Debug,
                    category: Some("database".to_string()),
                },
            ],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Plain,
                colored: Some(false),
            },
            emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                path: log_file.path().to_str().unwrap().to_string(),
            }),
        }]);

        ten_configure_log_reloadable(&config).unwrap();

        // Messages that should not be logged (level mismatch)
        debug!(target: "auth", "Auth debug message"); // Won't match: auth only allows info
        trace!(target: "database", "DB trace message"); // Won't match: database only allows debug

        // Messages that should not be logged (category not configured)
        info!(target: "network", "Network info message");
        debug!(target: "network", "Network debug message");

        // Messages that should not be logged (default category)
        info!("Default category info message");
        debug!("Default category debug message");

        // Read and verify log file contents
        let log_content = fs::read_to_string(log_file.path())
            .expect("Failed to read log file");

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
                    level: AdvancedLogLevel::Info,
                    category: Some("auth".to_string()),
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Plain,
                    colored: Some(false),
                },
                emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                    path: auth_file.path().to_str().unwrap().to_string(),
                }),
            },
            // Handler 2: Database logs (all levels) to db_file
            AdvancedLogHandler {
                matchers: vec![AdvancedLogMatcher {
                    level: AdvancedLogLevel::Debug,
                    category: Some("database".to_string()),
                }],
                formatter: AdvancedLogFormatter {
                    formatter_type: FormatterType::Plain,
                    colored: Some(false),
                },
                emitter: AdvancedLogEmitter::File(FileEmitterConfig {
                    path: db_file.path().to_str().unwrap().to_string(),
                }),
            },
        ]);

        ten_configure_log_reloadable(&config).unwrap();

        // Auth logs at different levels
        info!(target: "auth", "User login successful"); // Should appear in auth_file
        warn!(target: "auth", "Failed login attempt"); // Should appear in auth_file
        debug!(target: "auth", "Auth token details"); // Should NOT appear in auth_file

        // Database logs at different levels
        info!(target: "database", "Connection established"); // Should appear in db_file
        debug!(target: "database", "Query executed: SELECT * FROM users"); // Should appear in db_file
        debug!(target: "database", "Connection pool stats: 5 active"); // Should appear in db_file

        // Other category logs (should not appear in either file)
        info!(target: "network", "Server started");
        debug!(target: "network", "Socket initialized");

        // Force flush logs
        ten_configure_log_reloadable(&AdvancedLogConfig::new(vec![])).unwrap();

        // Read and verify auth file contents with backoff strategy
        let auth_content =
            read_with_backoff(auth_file.path().to_str().unwrap(), 0)
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
        assert!(
            !auth_content.contains("database"),
            "Auth file should not contain database logs"
        );
        assert!(
            !auth_content.contains("network"),
            "Auth file should not contain network logs"
        );

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
        assert!(
            !db_content.contains("auth"),
            "DB file should not contain auth logs"
        );
        assert!(
            !db_content.contains("network"),
            "DB file should not contain network logs"
        );
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
    fn test_actual_logging_output() {
        let config = AdvancedLogConfig::new(vec![AdvancedLogHandler {
            matchers: vec![AdvancedLogMatcher {
                level: AdvancedLogLevel::Trace,
                category: None,
            }],
            formatter: AdvancedLogFormatter {
                formatter_type: FormatterType::Json,
                colored: Some(true),
            },
            emitter: AdvancedLogEmitter::Console(ConsoleEmitterConfig {
                stream: StreamType::Stdout,
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
            LogLevel::Verbose,
            "parser",
            "json_parser.rs",
            100,
            "Parse JSON: {\"key\": \"value\"}",
        );
    }
}
