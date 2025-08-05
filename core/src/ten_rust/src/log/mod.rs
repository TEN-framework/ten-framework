//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod bindings;
pub mod formatter;
pub mod reloadable;

use serde::{Deserialize, Serialize};
use std::{fmt, io};
use tracing;
use tracing_appender::{non_blocking, rolling};
use tracing_subscriber::{
    fmt::{self as tracing_fmt},
    layer::SubscriberExt,
    util::SubscriberInitExt,
    Layer, Registry,
};

use crate::log::formatter::{
    JsonConfig, JsonFieldNames, JsonFormatter, PlainFormatter,
};
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
            LogLevel::Mandatory => tracing::Level::ERROR,
            LogLevel::Invalid => tracing::Level::ERROR,
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

#[derive(Debug, Serialize, Deserialize)]
pub struct AdvancedLogConfig {
    pub handlers: Vec<AdvancedLogHandler>,

    #[serde(skip)]
    guards: Vec<Box<dyn std::any::Any + Send + Sync>>,
}

impl AdvancedLogConfig {
    pub fn new(handlers: Vec<AdvancedLogHandler>) -> Self {
        Self { handlers, guards: Vec::new() }
    }

    pub fn add_guard(&mut self, guard: Box<dyn std::any::Any + Send + Sync>) {
        self.guards.push(guard);
    }
}

/// Creates a layer and filter separately from the handler configuration
///
/// This function is similar to `create_layer_from_handler` but returns the
/// layer and filter separately, allowing more flexibility in how they are
/// combined.
///
/// Returns a tuple containing:
/// - A boxed Layer that hasn't had any filter applied
/// - An EnvFilter configured according to the handler's matchers
fn create_layer_and_filter(
    handler: &AdvancedLogHandler,
) -> (LayerWithGuard, tracing_subscriber::EnvFilter) {
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

    let filter = tracing_subscriber::EnvFilter::try_new(&filter_directive)
        .unwrap_or_else(|_| {
            tracing_subscriber::EnvFilter::new("info") // Default fallback to
                                                       // info level
        });

    // Create corresponding layer based on emitter type
    let layer_with_guard = match &handler.emitter {
        AdvancedLogEmitter::Console(console_config) => {
            let layer = match (
                &console_config.stream,
                &handler.formatter.formatter_type,
            ) {
                (StreamType::Stdout, FormatterType::Plain) => {
                    let ansi = handler.formatter.colored.unwrap_or(false);
                    tracing_fmt::Layer::new()
                        .event_format(PlainFormatter::new(ansi))
                        .with_writer(io::stdout)
                        .boxed()
                }
                (StreamType::Stderr, FormatterType::Plain) => {
                    let ansi = handler.formatter.colored.unwrap_or(false);
                    tracing_fmt::Layer::new()
                        .event_format(PlainFormatter::new(ansi))
                        .with_writer(io::stderr)
                        .boxed()
                }
                (StreamType::Stdout, FormatterType::Json) => {
                    tracing_fmt::Layer::new()
                        .event_format(JsonFormatter::new(JsonConfig {
                            ansi: handler.formatter.colored.unwrap_or(false),
                            pretty: false,
                            field_names: JsonFieldNames::default(),
                        }))
                        .with_writer(io::stdout)
                        .boxed()
                }
                (StreamType::Stderr, FormatterType::Json) => {
                    tracing_fmt::Layer::new()
                        .event_format(JsonFormatter::new(JsonConfig {
                            ansi: handler.formatter.colored.unwrap_or(false),
                            pretty: false,
                            field_names: JsonFieldNames::default(),
                        }))
                        .with_writer(io::stderr)
                        .boxed()
                }
            };
            LayerWithGuard { layer, guard: None }
        }
        AdvancedLogEmitter::File(file_config) => {
            // Create file appender for file logging
            let file_appender = rolling::never(".", &file_config.path);
            let (non_blocking, guard) = non_blocking(file_appender);

            let layer = match handler.formatter.formatter_type {
                FormatterType::Plain => {
                    tracing_fmt::Layer::new()
                        .event_format(PlainFormatter::new(
                            handler.formatter.colored.unwrap_or(false),
                        )) // File output doesn't need colors
                        .with_writer(non_blocking)
                        .boxed()
                }
                FormatterType::Json => tracing_fmt::Layer::new()
                    .event_format(JsonFormatter::new(JsonConfig {
                        ansi: handler.formatter.colored.unwrap_or(false),
                        pretty: false,
                        field_names: JsonFieldNames::default(),
                    }))
                    .with_writer(non_blocking)
                    .boxed(),
            };

            LayerWithGuard { layer, guard: Some(Box::new(guard)) }
        }
    };

    (layer_with_guard, filter)
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

fn ten_configure_log_non_reloadable(
    config: &mut AdvancedLogConfig,
) -> Result<(), LogInitError> {
    let mut layers = Vec::with_capacity(config.handlers.len());
    let mut guards = Vec::new();

    // Create layers from handlers
    {
        let handlers = &config.handlers;
        for handler in handlers {
            let (layer_with_guard, filter) = create_layer_and_filter(handler);
            if let Some(guard) = layer_with_guard.guard {
                guards.push(guard);
            }
            layers.push(layer_with_guard.layer.with_filter(filter).boxed());
        }
    }

    // Add guards to config
    for guard in guards {
        config.add_guard(guard);
    }

    // Initialize the registry
    tracing_subscriber::registry().with(layers).try_init().map_err(|_| {
        LogInitError { message: "Logging system is already initialized" }
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
