//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod bindings;
pub mod decrypt;
pub mod dynamic_filter;
pub mod encryption;
pub mod file_appender;
pub mod formatter;
pub mod reloadable;

use std::{fmt, io};

use serde::{Deserialize, Serialize};
use tracing;
use tracing_appender::non_blocking;
use tracing_subscriber::{
    fmt::{
        writer::BoxMakeWriter,
        {self as tracing_fmt},
    },
    layer::SubscriberExt,
    util::SubscriberInitExt,
    Layer, Registry,
};

use crate::log::{
    dynamic_filter::DynamicTargetFilterLayer,
    encryption::{EncryptMakeWriter, EncryptionConfig},
    file_appender::FileAppenderGuard,
    formatter::{JsonConfig, JsonFieldNames, JsonFormatter, PlainFormatter},
};

// Encryption types and writer are moved to `encryption.rs`
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(from = "u8")]
pub enum LogLevel {
    Invalid = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
}

impl From<u8> for LogLevel {
    fn from(value: u8) -> Self {
        match value {
            0 => LogLevel::Invalid,
            1 => LogLevel::Debug,
            2 => LogLevel::Info,
            3 => LogLevel::Warn,
            4 => LogLevel::Error,
            _ => LogLevel::Invalid,
        }
    }
}

impl LogLevel {
    fn to_tracing_level(&self) -> tracing::Level {
        match self {
            LogLevel::Debug => tracing::Level::DEBUG,
            LogLevel::Info => tracing::Level::INFO,
            LogLevel::Warn => tracing::Level::WARN,
            LogLevel::Error => tracing::Level::ERROR,
            LogLevel::Invalid => tracing::Level::ERROR,
        }
    }
}

// Advanced log level enum that serializes to/from strings
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum AdvancedLogLevelFilter {
    OFF,
    Debug,
    Info,
    Warn,
    Error,
}

impl fmt::Display for AdvancedLogLevelFilter {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Self::OFF => "off",
            Self::Debug => "debug",
            Self::Info => "info",
            Self::Warn => "warn",
            Self::Error => "error",
        })
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AdvancedLogMatcher {
    pub level: AdvancedLogLevelFilter,
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
    #[serde(skip_serializing_if = "Option::is_none")]
    pub encryption: Option<EncryptionConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FileEmitterConfig {
    pub path: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub encryption: Option<EncryptionConfig>,
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

#[derive(Debug, Serialize, Deserialize)]
pub struct AdvancedLogConfig {
    pub handlers: Vec<AdvancedLogHandler>,

    #[serde(skip)]
    guards: Vec<Box<dyn std::any::Any + Send + Sync>>,
}

impl AdvancedLogConfig {
    pub fn new(handlers: Vec<AdvancedLogHandler>) -> Self {
        Self {
            handlers,
            guards: Vec::new(),
        }
    }

    pub fn add_guard(&mut self, guard: Box<dyn std::any::Any + Send + Sync>) {
        self.guards.push(guard);
    }
}

/// Creates a layer with dynamic category filtering from the handler
/// configuration
///
/// This function creates a layer that can filter based on the category field
/// in log event fields, not just the static category.
///
/// Returns:
/// - A LayerWithGuard containing the filtering layer
fn create_layer_with_dynamic_filter(handler: &AdvancedLogHandler) -> LayerWithGuard {
    // Create base layer based on emitter type (without filtering)
    let base_layer_with_guard = match &handler.emitter {
        AdvancedLogEmitter::Console(console_config) => {
            let layer = match (&console_config.stream, &handler.formatter.formatter_type) {
                (StreamType::Stdout, FormatterType::Plain) => {
                    let ansi = handler.formatter.colored.unwrap_or(false);
                    let base_writer = io::stdout;
                    let writer = if let Some(runtime) =
                        console_config.encryption.as_ref().and_then(|e| e.to_runtime())
                    {
                        BoxMakeWriter::new(EncryptMakeWriter {
                            inner: base_writer,
                            runtime,
                        })
                    } else {
                        BoxMakeWriter::new(base_writer)
                    };
                    tracing_fmt::Layer::new()
                        .event_format(PlainFormatter::new(ansi))
                        .with_writer(writer)
                        .boxed()
                }
                (StreamType::Stderr, FormatterType::Plain) => {
                    let ansi = handler.formatter.colored.unwrap_or(false);
                    let base_writer = io::stderr;
                    let writer = if let Some(runtime) =
                        console_config.encryption.as_ref().and_then(|e| e.to_runtime())
                    {
                        BoxMakeWriter::new(EncryptMakeWriter {
                            inner: base_writer,
                            runtime,
                        })
                    } else {
                        BoxMakeWriter::new(base_writer)
                    };
                    tracing_fmt::Layer::new()
                        .event_format(PlainFormatter::new(ansi))
                        .with_writer(writer)
                        .boxed()
                }
                (StreamType::Stdout, FormatterType::Json) => {
                    let base_writer = io::stdout;
                    let writer = if let Some(runtime) =
                        console_config.encryption.as_ref().and_then(|e| e.to_runtime())
                    {
                        BoxMakeWriter::new(EncryptMakeWriter {
                            inner: base_writer,
                            runtime,
                        })
                    } else {
                        BoxMakeWriter::new(base_writer)
                    };
                    tracing_fmt::Layer::new()
                        .event_format(JsonFormatter::new(JsonConfig {
                            ansi: handler.formatter.colored.unwrap_or(false),
                            pretty: false,
                            field_names: JsonFieldNames::default(),
                        }))
                        .with_writer(writer)
                        .boxed()
                }
                (StreamType::Stderr, FormatterType::Json) => {
                    let base_writer = io::stderr;
                    let writer = if let Some(runtime) =
                        console_config.encryption.as_ref().and_then(|e| e.to_runtime())
                    {
                        BoxMakeWriter::new(EncryptMakeWriter {
                            inner: base_writer,
                            runtime,
                        })
                    } else {
                        BoxMakeWriter::new(base_writer)
                    };
                    tracing_fmt::Layer::new()
                        .event_format(JsonFormatter::new(JsonConfig {
                            ansi: handler.formatter.colored.unwrap_or(false),
                            pretty: false,
                            field_names: JsonFieldNames::default(),
                        }))
                        .with_writer(writer)
                        .boxed()
                }
            };
            LayerWithGuard {
                layer,
                guard: None,
            }
        }
        AdvancedLogEmitter::File(file_config) => {
            // Create our reloadable file appender. It supports CAS-based
            // reopen.
            let appender = file_appender::ReloadableFileAppender::new(&file_config.path);
            let (non_blocking, worker_guard) = non_blocking(appender.clone());
            // keep both worker_guard and appender in a composite guard
            let composite_guard = file_appender::FileAppenderGuard {
                non_blocking_guard: worker_guard,
                appender: appender.clone(),
            };

            let layer = match handler.formatter.formatter_type {
                FormatterType::Plain => {
                    let writer = if let Some(runtime) =
                        file_config.encryption.as_ref().and_then(|e| e.to_runtime())
                    {
                        BoxMakeWriter::new(EncryptMakeWriter {
                            inner: non_blocking.clone(),
                            runtime,
                        })
                    } else {
                        BoxMakeWriter::new(non_blocking.clone())
                    };
                    tracing_fmt::Layer::new()
                        .event_format(PlainFormatter::new(
                            handler.formatter.colored.unwrap_or(false),
                        )) // File output doesn't need colors
                        .with_writer(writer)
                        .boxed()
                }
                FormatterType::Json => {
                    let writer = if let Some(runtime) =
                        file_config.encryption.as_ref().and_then(|e| e.to_runtime())
                    {
                        BoxMakeWriter::new(EncryptMakeWriter {
                            inner: non_blocking.clone(),
                            runtime,
                        })
                    } else {
                        BoxMakeWriter::new(non_blocking.clone())
                    };
                    tracing_fmt::Layer::new()
                        .event_format(JsonFormatter::new(JsonConfig {
                            ansi: handler.formatter.colored.unwrap_or(false),
                            pretty: false,
                            field_names: JsonFieldNames::default(),
                        }))
                        .with_writer(writer)
                        .boxed()
                }
            };

            LayerWithGuard {
                layer,
                guard: Some(Box::new(composite_guard)),
            }
        }
    };

    // Wrap the base layer with our dynamic category filter
    let filtered_layer =
        DynamicTargetFilterLayer::new(base_layer_with_guard.layer, handler.matchers.clone());

    LayerWithGuard {
        layer: Box::new(filtered_layer),
        guard: base_layer_with_guard.guard,
    }
}

/// A wrapper for a logging layer that may have an associated guard
pub(crate) struct LayerWithGuard {
    /// The actual logging layer
    pub layer: Box<dyn Layer<Registry> + Send + Sync>,
    /// Optional guard that must be kept alive for the layer to function
    /// properly (particularly for non-blocking file writers)
    pub guard: Option<Box<dyn std::any::Any + Send + Sync>>,
}

/// Error type for logging initialization
#[derive(Debug)]
pub struct LogInitError {
    message: &'static str,
}

impl std::fmt::Display for LogInitError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for LogInitError {}

fn ten_configure_log_non_reloadable(config: &mut AdvancedLogConfig) -> Result<(), LogInitError> {
    let mut layers = Vec::with_capacity(config.handlers.len());
    let mut guards = Vec::new();

    // Create layers from handlers
    {
        let handlers = &config.handlers;
        for handler in handlers {
            let layer_with_guard = create_layer_with_dynamic_filter(handler);
            if let Some(guard) = layer_with_guard.guard {
                guards.push(guard);
            }
            layers.push(layer_with_guard.layer);
        }
    }

    // Add guards to config
    for guard in guards {
        config.add_guard(guard);
    }

    // Initialize the registry
    tracing_subscriber::registry().with(layers).try_init().map_err(|_| LogInitError {
        message: "Logging system is already initialized",
    })
}

/// Configure the logging system for production use
///
/// This function initializes the logging system with the provided
/// configuration. It can only be called once - subsequent calls will result in
/// an error.
///
/// # Arguments
/// * `config` - The logging configuration
///
/// # Returns
/// * `Ok(())` if initialization was successful
///
/// * `Err(LogInitError)` if the logging system was already initialized
pub fn ten_configure_log(
    config: &mut AdvancedLogConfig,
    reloadable: bool,
) -> Result<(), LogInitError> {
    if reloadable {
        reloadable::ten_configure_log_reloadable(config)
    } else {
        ten_configure_log_non_reloadable(config)
    }
}

/// Trigger reopen for all file appenders (applied on next write).
/// - Non-reloadable: iterate current config guards and trigger
///   `FileAppenderGuard`.
/// - Reloadable: delegate to the reloadable log manager.
pub fn ten_log_reopen_all(config: &mut AdvancedLogConfig, reloadable: bool) {
    if reloadable {
        reloadable::request_reopen_all_files();
        return;
    }

    // Non-reloadable: iterate config.guards directly
    for any_guard in config.guards.iter() {
        if let Some(file_guard) = any_guard.downcast_ref::<FileAppenderGuard>() {
            file_guard.request_reopen();
        }
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
    let filename =
        std::path::Path::new(file_name).file_name().and_then(|n| n.to_str()).unwrap_or(file_name);

    match tracing_level {
        tracing::Level::TRACE => {
            tracing::trace!(
                category = category,
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
                category = category,
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
                category = category,
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
                category = category,
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
                category = category,
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
