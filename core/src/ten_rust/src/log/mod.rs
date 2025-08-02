//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod bindings;
pub mod formatter;

use formatter::PlainFormatter;
use serde::{Deserialize, Serialize};
use std::fmt;
use std::io;
use tracing;
use tracing_appender::{non_blocking, rolling};
use tracing_subscriber::{
    fmt::{self as tracing_fmt},
    layer::SubscriberExt,
    util::SubscriberInitExt,
    EnvFilter, Layer, Registry,
};

use crate::log::formatter::JsonConfig;
use crate::log::formatter::JsonFieldNames;
use crate::log::formatter::JsonFormatter;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(from = "u8")]
pub enum LogLevel {
    Invalid = 0,
    Verbose = 1,
    Debug = 2,
    Info = 3,
    Warn = 4,
    Error = 5,
    Fatal = 6,
    Mandatory = 7,
}

impl From<u8> for LogLevel {
    fn from(value: u8) -> Self {
        match value {
            0 => LogLevel::Invalid,
            1 => LogLevel::Verbose,
            2 => LogLevel::Debug,
            3 => LogLevel::Info,
            4 => LogLevel::Warn,
            5 => LogLevel::Error,
            6 => LogLevel::Fatal,
            7 => LogLevel::Mandatory,
            _ => LogLevel::Invalid,
        }
    }
}

impl LogLevel {
    fn to_tracing_level(&self) -> tracing::Level {
        match self {
            LogLevel::Verbose => tracing::Level::TRACE,
            LogLevel::Debug => tracing::Level::DEBUG,
            LogLevel::Info => tracing::Level::INFO,
            LogLevel::Warn => tracing::Level::WARN,
            LogLevel::Error => tracing::Level::ERROR,
            LogLevel::Fatal => tracing::Level::ERROR,
            LogLevel::Mandatory => tracing::Level::INFO,
            LogLevel::Invalid => tracing::Level::TRACE,
        }
    }
}

// Advanced log level enum that serializes to/from strings
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum AdvancedLogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
}

impl fmt::Display for AdvancedLogLevel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Self::Trace => "trace",
            Self::Debug => "debug",
            Self::Info => "info",
            Self::Warn => "warn",
            Self::Error => "error",
        })
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AdvancedLogMatcher {
    pub level: AdvancedLogLevel,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub category: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum FormatterType {
    Plain,
    Json,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AdvancedLogFormatter {
    #[serde(rename = "type")]
    pub formatter_type: FormatterType,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub colored: Option<bool>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum StreamType {
    Stdout,
    Stderr,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ConsoleEmitterConfig {
    pub stream: StreamType,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FileEmitterConfig {
    pub path: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(tag = "type", content = "config")]
#[serde(rename_all = "lowercase")]
pub enum AdvancedLogEmitter {
    Console(ConsoleEmitterConfig),
    File(FileEmitterConfig),
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AdvancedLogHandler {
    pub matchers: Vec<AdvancedLogMatcher>,
    pub formatter: AdvancedLogFormatter,
    pub emitter: AdvancedLogEmitter,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AdvancedLogConfig {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub level: Option<AdvancedLogLevel>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub handlers: Option<Vec<AdvancedLogHandler>>,
}

/// Configure logging system using tracing library based on AdvancedLogConfig
///
/// # Features
/// - Support for multiple log handlers
/// - Filter logs by level and category
/// - Support for plain and JSON format output
/// - Support for console (stdout/stderr) and file output
/// - Support for colored output control
///
/// # Notes
/// - This function sets the global tracing subscriber and should only be called
///   once
/// - For file output, it's recommended to keep a reference to the guard
///   throughout the application lifecycle
/// - If no handlers are configured, default console output configuration will
///   be used
pub fn ten_configure_log(config: &AdvancedLogConfig) {
    // Create base registry
    let registry = Registry::default();

    // If no handlers are configured, use default configuration
    let handlers = match &config.handlers {
        Some(handlers) if !handlers.is_empty() => handlers,
        _ => {
            // Default configuration: output to stdout, use plain format
            let default_config = AdvancedLogConfig {
                level: config.level.clone(),
                handlers: Some(vec![AdvancedLogHandler {
                    matchers: vec![AdvancedLogMatcher {
                        level: config
                            .level
                            .clone()
                            .unwrap_or(AdvancedLogLevel::Info),
                        category: None,
                    }],
                    formatter: AdvancedLogFormatter {
                        formatter_type: FormatterType::Plain,
                        colored: Some(true),
                    },
                    emitter: AdvancedLogEmitter::Console(
                        ConsoleEmitterConfig { stream: StreamType::Stdout },
                    ),
                }]),
            };
            return ten_configure_log(&default_config);
        }
    };

    let mut layers: Vec<Box<dyn Layer<Registry> + Send + Sync>> = Vec::new();

    // Create corresponding layer for each handler
    for handler in handlers {
        // Create filter
        let mut filter_directive = String::new();

        // Build filter rules based on matchers
        for (i, matcher) in handler.matchers.iter().enumerate() {
            if i > 0 {
                filter_directive.push(',');
            }

            let level_str = matcher.level.to_string();

            if let Some(category) = &matcher.category {
                filter_directive.push_str(&format!("{category}={level_str}"));
            } else {
                filter_directive.push_str(&level_str);
            }
        }

        let filter =
            EnvFilter::try_new(&filter_directive).unwrap_or_else(|_| {
                EnvFilter::new("info") // Default fallback to info level
            });

        // Create corresponding layer based on emitter type
        match &handler.emitter {
            AdvancedLogEmitter::Console(console_config) => {
                let layer: Box<dyn Layer<Registry> + Send + Sync> = match (
                    &console_config.stream,
                    &handler.formatter.formatter_type,
                ) {
                    (StreamType::Stdout, FormatterType::Plain) => {
                        let ansi = handler.formatter.colored.unwrap_or(false);
                        tracing_fmt::Layer::new()
                            .event_format(PlainFormatter::new(ansi))
                            .with_writer(io::stdout)
                            .with_ansi(ansi)
                            .with_filter(filter)
                            .boxed()
                    }
                    (StreamType::Stderr, FormatterType::Plain) => {
                        let ansi = handler.formatter.colored.unwrap_or(false);
                        tracing_fmt::Layer::new()
                            .event_format(PlainFormatter::new(ansi))
                            .with_writer(io::stderr)
                            .with_ansi(ansi)
                            .with_filter(filter)
                            .boxed()
                    }
                    (StreamType::Stdout, FormatterType::Json) => {
                        tracing_fmt::Layer::new()
                            .event_format(JsonFormatter::new(JsonConfig {
                                ansi: handler
                                    .formatter
                                    .colored
                                    .unwrap_or(false),
                                pretty: false,
                                field_names: JsonFieldNames::default(),
                            }))
                            .with_ansi(
                                handler.formatter.colored.unwrap_or(false),
                            )
                            .with_writer(io::stdout)
                            .with_filter(filter)
                            .boxed()
                    }
                    (StreamType::Stderr, FormatterType::Json) => {
                        tracing_fmt::Layer::new()
                            .event_format(JsonFormatter::new(JsonConfig {
                                ansi: handler
                                    .formatter
                                    .colored
                                    .unwrap_or(false),
                                pretty: false,
                                field_names: JsonFieldNames::default(),
                            }))
                            .with_ansi(
                                handler.formatter.colored.unwrap_or(false),
                            )
                            .with_writer(io::stderr)
                            .with_filter(filter)
                            .boxed()
                    }
                };

                layers.push(layer);
            }
            AdvancedLogEmitter::File(file_config) => {
                // Create file appender for file logging
                let file_appender = rolling::never(".", &file_config.path);
                let (non_blocking, _guard) = non_blocking(file_appender);

                let layer = match handler.formatter.formatter_type {
                    FormatterType::Plain => {
                        tracing_fmt::Layer::new()
                            .event_format(PlainFormatter::new(false)) // File output doesn't need colors
                            .with_writer(non_blocking)
                            .with_ansi(false)
                            .with_filter(filter)
                            .boxed()
                    }
                    FormatterType::Json => tracing_fmt::Layer::new()
                        .event_format(JsonFormatter::new(JsonConfig {
                            ansi: handler.formatter.colored.unwrap_or(false),
                            pretty: false,
                            field_names: JsonFieldNames::default(),
                        }))
                        .with_writer(non_blocking)
                        .with_filter(filter)
                        .boxed(),
                };

                layers.push(layer);

                // Note: _guard is dropped here, but in actual applications it
                // should be saved to ensure non_blocking writer
                // works properly
                std::mem::forget(_guard);
            }
        }
    }

    // Combine all layers and initialize global subscriber
    let subscriber = registry.with(layers);

    // Set global default subscriber
    if let Err(e) = subscriber.try_init() {
        eprintln!("Failed to set global default subscriber: {e}");
    }
}

#[allow(clippy::too_many_arguments)]
pub fn ten_log(
    _config: &AdvancedLogConfig,
    category: &str,
    pid: i64,
    tid: i64,
    level: LogLevel,
    func_name: &str,
    file_name: &str,
    line_no: u32,
    msg: &str,
) {
    let tracing_level = level.to_tracing_level();

    // Extract just the filename from the full path
    let filename = std::path::Path::new(file_name)
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or(file_name);

    match tracing_level {
        tracing::Level::TRACE => {
            tracing::trace!(
                target = category,
                pid = pid,
                tid = tid,
                func_name = func_name,
                file_name = filename,
                line_no = line_no,
                "{}",
                msg
            )
        }
        tracing::Level::DEBUG => {
            tracing::debug!(
                target = category,
                pid = pid,
                tid = tid,
                func_name = func_name,
                file_name = filename,
                line_no = line_no,
                "{}",
                msg
            )
        }
        tracing::Level::INFO => {
            tracing::info!(
                target = category,
                pid = pid,
                tid = tid,
                func_name = func_name,
                file_name = filename,
                line_no = line_no,
                "{}",
                msg
            )
        }
        tracing::Level::WARN => {
            tracing::warn!(
                target = category,
                pid = pid,
                tid = tid,
                func_name = func_name,
                file_name = filename,
                line_no = line_no,
                "{}",
                msg
            )
        }
        tracing::Level::ERROR => {
            tracing::error!(
                target = category,
                pid = pid,
                tid = tid,
                func_name = func_name,
                file_name = filename,
                line_no = line_no,
                "{}",
                msg
            )
        }
    }
}
