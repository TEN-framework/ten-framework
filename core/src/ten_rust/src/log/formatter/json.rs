//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fmt;

use tracing::{Event, Level, Subscriber};
use tracing_subscriber::{
    field::Visit,
    fmt::{format, FmtContext, FormatEvent, FormatFields},
    registry::LookupSpan,
};

// ANSI color codes
const COLOR_RESET: &str = "\x1b[0m";
const COLOR_RED: &str = "\x1b[31m";
const COLOR_GREEN: &str = "\x1b[32m";
const COLOR_YELLOW: &str = "\x1b[33m";
const COLOR_BLUE: &str = "\x1b[34m";
const COLOR_MAGENTA: &str = "\x1b[35m";
const COLOR_CYAN: &str = "\x1b[36m";

#[derive(Default)]
struct FieldVisitor {
    pid: Option<i64>,
    tid: Option<i64>,
    func_name: Option<String>,
    file_name: Option<String>,
    line_no: Option<u32>,
    message: String,
    category: Option<String>,
}

impl Visit for FieldVisitor {
    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn fmt::Debug) {
        match field.name() {
            "pid" => {
                if let Ok(pid) = format!("{value:?}").parse::<i64>() {
                    self.pid = Some(pid);
                }
            }
            "tid" => {
                if let Ok(tid) = format!("{value:?}").parse::<i64>() {
                    self.tid = Some(tid);
                }
            }
            "func_name" => {
                self.func_name = Some(format!("{value:?}").trim_matches('"').to_string());
            }
            "file_name" => {
                self.file_name = Some(format!("{value:?}").trim_matches('"').to_string());
            }
            "line_no" => {
                if let Ok(line) = format!("{value:?}").parse::<u32>() {
                    self.line_no = Some(line);
                }
            }
            "category" => {
                self.category = Some(format!("{value:?}").trim_matches('"').to_string());
            }
            "message" => {
                if !self.message.is_empty() {
                    self.message.push(' ');
                }
                self.message.push_str(format!("{value:?}").trim_matches('"'));
            }
            _ => {
                // This might be the actual log message
                if field.name() == "message" || self.message.is_empty() {
                    if !self.message.is_empty() {
                        self.message.push(' ');
                    }
                    self.message.push_str(format!("{value:?}").trim_matches('"'));
                }
            }
        }
    }

    fn record_str(&mut self, field: &tracing::field::Field, value: &str) {
        match field.name() {
            "func_name" => {
                self.func_name = Some(value.to_string());
            }
            "file_name" => {
                self.file_name = Some(value.to_string());
            }
            "category" => {
                self.category = Some(value.to_string());
            }
            "message" => {
                if !self.message.is_empty() {
                    self.message.push(' ');
                }
                self.message.push_str(value);
            }
            _ => {
                // This might be the actual log message
                if self.message.is_empty() {
                    self.message.push_str(value);
                }
            }
        }
    }

    fn record_u64(&mut self, field: &tracing::field::Field, value: u64) {
        if field.name() == "line_no" {
            self.line_no = Some(value as u32);
        }
    }

    fn record_i64(&mut self, field: &tracing::field::Field, value: i64) {
        match field.name() {
            "pid" => {
                self.pid = Some(value);
            }
            "tid" => {
                self.tid = Some(value);
            }
            _ => {}
        }
    }
}

/// Configuration for JSON formatter
#[derive(Debug, Clone, Default)]
pub struct JsonConfig {
    /// Whether to use ANSI colors in output
    pub ansi: bool,
    /// Whether to pretty print JSON
    pub pretty: bool,
    /// Custom field names for JSON output
    pub field_names: JsonFieldNames,
}

/// Custom field names for JSON output
#[derive(Debug, Clone)]
pub struct JsonFieldNames {
    pub timestamp: String,
    pub level: String,
    pub pid: String,
    pub tid: String,
    pub category: String,
    pub function: String,
    pub file: String,
    pub line: String,
    pub message: String,
}

impl Default for JsonFieldNames {
    fn default() -> Self {
        Self {
            timestamp: "timestamp".to_string(),
            level: "level".to_string(),
            pid: "pid".to_string(),
            tid: "tid".to_string(),
            category: "category".to_string(),
            function: "function".to_string(),
            file: "file".to_string(),
            line: "line".to_string(),
            message: "message".to_string(),
        }
    }
}

/// Custom formatter for JSON output
pub struct JsonFormatter {
    config: JsonConfig,
}

impl JsonFormatter {
    pub fn new(config: JsonConfig) -> Self {
        Self {
            config,
        }
    }

    fn get_level_color(&self, level: &Level) -> &'static str {
        if !self.config.ansi {
            return "";
        }
        match *level {
            Level::ERROR => COLOR_RED,
            Level::WARN => COLOR_YELLOW,
            Level::INFO => COLOR_GREEN,
            Level::DEBUG => COLOR_CYAN,
            Level::TRACE => COLOR_CYAN,
        }
    }

    fn format_time(&self, writer: &mut dyn fmt::Write) -> fmt::Result {
        let now = chrono::Utc::now();
        write!(writer, "{}", now.to_rfc3339())
    }
}

impl<S, N> FormatEvent<S, N> for JsonFormatter
where
    S: Subscriber + for<'a> LookupSpan<'a>,
    N: for<'a> FormatFields<'a> + 'static,
{
    fn format_event(
        &self,
        _ctx: &FmtContext<'_, S, N>,
        mut writer: format::Writer<'_>,
        event: &Event<'_>,
    ) -> fmt::Result {
        let metadata = event.metadata();
        let mut visitor = FieldVisitor::default();
        event.record(&mut visitor);

        // Start JSON object
        write!(writer, "{{")?;

        // Timestamp
        write!(
            writer,
            "\"{}\":\"{}",
            self.config.field_names.timestamp,
            if self.config.ansi { COLOR_BLUE } else { "" }
        )?;
        self.format_time(&mut writer)?;
        write!(writer, "{}\"", if self.config.ansi { COLOR_RESET } else { "" })?;

        // Level
        let level_color = self.get_level_color(metadata.level());
        write!(
            writer,
            ",\"{}\":\"{}{}{}\"",
            self.config.field_names.level,
            level_color,
            metadata.level(),
            if self.config.ansi { COLOR_RESET } else { "" }
        )?;

        // Category
        let category = visitor.category.as_ref().map_or(metadata.target(), |v| v);
        write!(
            writer,
            ",\"{}\":\"{}{}{}\"",
            self.config.field_names.category,
            if self.config.ansi { COLOR_MAGENTA } else { "" },
            category,
            if self.config.ansi { COLOR_RESET } else { "" }
        )?;

        // PID and TID
        let pid = visitor.pid.unwrap_or(0);
        let tid = visitor.tid.unwrap_or(0);
        write!(
            writer,
            ",\"{}\":{}{}{}",
            self.config.field_names.pid,
            if self.config.ansi { COLOR_CYAN } else { "" },
            pid,
            if self.config.ansi { COLOR_RESET } else { "" }
        )?;
        write!(
            writer,
            ",\"{}\":{}{}{}",
            self.config.field_names.tid,
            if self.config.ansi { COLOR_CYAN } else { "" },
            tid,
            if self.config.ansi { COLOR_RESET } else { "" }
        )?;

        // Function name
        if let Some(func_name) = visitor.func_name {
            write!(
                writer,
                ",\"{}\":\"{}{}{}\"",
                self.config.field_names.function,
                if self.config.ansi { COLOR_MAGENTA } else { "" },
                func_name,
                if self.config.ansi { COLOR_RESET } else { "" }
            )?;
        }

        // File and line
        if let Some(file_name) = visitor.file_name {
            write!(
                writer,
                ",\"{}\":\"{}{}{}\"",
                self.config.field_names.file,
                if self.config.ansi { COLOR_BLUE } else { "" },
                file_name,
                if self.config.ansi { COLOR_RESET } else { "" }
            )?;
            if let Some(line_no) = visitor.line_no {
                write!(
                    writer,
                    ",\"{}\":{}{}{}",
                    self.config.field_names.line,
                    if self.config.ansi { COLOR_CYAN } else { "" },
                    line_no,
                    if self.config.ansi { COLOR_RESET } else { "" }
                )?;
            }
        }

        // Message
        write!(
            writer,
            ",\"{}\":\"{}{}{}\"",
            self.config.field_names.message,
            if self.config.ansi { COLOR_YELLOW } else { "" },
            visitor.message.replace('"', "\\\""),
            if self.config.ansi { COLOR_RESET } else { "" }
        )?;

        // End JSON object
        write!(writer, "}}")?;
        writeln!(writer)?;

        Ok(())
    }
}
