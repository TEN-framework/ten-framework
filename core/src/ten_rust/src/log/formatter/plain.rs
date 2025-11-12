//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::fmt;

use tracing::{Event, Subscriber};
use tracing_subscriber::{
    field::Visit,
    fmt::{format, FmtContext, FormatEvent, FormatFields},
    registry::LookupSpan,
};

fn level_to_char(level: &tracing::Level) -> char {
    match *level {
        tracing::Level::TRACE => 'V',
        tracing::Level::DEBUG => 'D',
        tracing::Level::INFO => 'I',
        tracing::Level::WARN => 'W',
        tracing::Level::ERROR => 'E',
    }
}

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

// ANSI color codes
const RESET: &str = "\x1b[0m";
const RED: &str = "\x1b[31m";
const YELLOW: &str = "\x1b[33m";
const GREEN: &str = "\x1b[32m";
const BLUE: &str = "\x1b[34m";
const MAGENTA: &str = "\x1b[35m";
const CYAN: &str = "\x1b[36m";
const GRAY: &str = "\x1b[37m";

/// Custom formatter to match the C plain formatter output
pub struct PlainFormatter {
    ansi: bool,
}

impl PlainFormatter {
    pub fn new(ansi: bool) -> Self {
        Self {
            ansi,
        }
    }

    fn level_color(&self, level: &tracing::Level) -> &'static str {
        if !self.ansi {
            return "";
        }
        match *level {
            // Match C implementation's color scheme
            tracing::Level::ERROR => RED,   // ERROR and FATAL
            tracing::Level::WARN => YELLOW, // WARN
            tracing::Level::INFO => GREEN,  /* INFO and MANDATORY (though */
            // MANDATORY should be GOLD)
            tracing::Level::DEBUG => CYAN, // DEBUG
            tracing::Level::TRACE => CYAN, // VERBOSE
        }
    }

    fn reset_color(&self) -> &'static str {
        if !self.ansi {
            return "";
        }
        RESET
    }
}

impl<S, N> FormatEvent<S, N> for PlainFormatter
where
    S: Subscriber + for<'a> LookupSpan<'a>,
    N: for<'a> FormatFields<'a> + 'static,
{
    fn format_event(
        &self,
        ctx: &FmtContext<'_, S, N>,
        mut writer: format::Writer<'_>,
        event: &Event<'_>,
    ) -> fmt::Result {
        let metadata = event.metadata();

        // Time - using ISO 8601 format
        use chrono::Utc;
        let now = Utc::now();
        write!(writer, "{}", now.to_rfc3339())?;

        // Extract fields from the event first
        let mut visitor = FieldVisitor::default();
        event.record(&mut visitor);

        // PID(TID) - use values from C side
        let pid = visitor.pid.unwrap_or(0);
        let tid = visitor.tid.unwrap_or(0);
        let category = visitor.category.as_ref().map_or(metadata.target(), |v| v);
        write!(writer, " {pid}({tid}) ")?;

        // Level with color
        let level = metadata.level();
        let level_char = level_to_char(level);
        let color = self.level_color(level);
        write!(writer, "{color}{level_char}{}", self.reset_color())?;

        // Category
        if !category.is_empty() {
            if self.ansi {
                write!(
                    writer,
                    " {magenta}{}{reset}",
                    category,
                    magenta = CYAN,
                    reset = self.reset_color()
                )?;
            } else {
                write!(writer, " {category}")?;
            }
        }

        // Format function@file:line using extracted fields with colors
        if let (Some(func_name), Some(file_name), Some(line_no)) =
            (visitor.func_name.as_ref(), visitor.file_name.as_ref(), visitor.line_no)
        {
            if self.ansi {
                write!(
                    writer,
                    " {magenta}{func_name}{reset}@{blue}{file_name}:{line_no}{reset}",
                    magenta = MAGENTA,
                    reset = self.reset_color(),
                    blue = BLUE
                )?;
            } else {
                write!(writer, " {func_name}@{file_name}:{line_no}")?;
            }
        } else if let Some(file) = metadata.file() {
            // Fallback to tracing's built-in metadata
            let line = metadata.line().unwrap_or(0);
            let filename =
                std::path::Path::new(file).file_name().and_then(|n| n.to_str()).unwrap_or(file);

            if self.ansi {
                write!(
                    writer,
                    " @{blue}{filename}:{line}{reset}",
                    blue = BLUE,
                    reset = self.reset_color()
                )?;
            } else {
                write!(writer, " @{filename}:{line}")?;
            }
        }

        // Message - only output the message part with color
        write!(writer, " ")?;
        if !visitor.message.is_empty() {
            if self.ansi {
                write!(
                    writer,
                    "{white}{}{reset}",
                    visitor.message,
                    white = GRAY, /* Using GRAY as white to match C
                                   * implementation */
                    reset = self.reset_color()
                )?;
            } else {
                write!(writer, "{}", visitor.message)?;
            }
        } else {
            // Fallback to default field formatting
            if self.ansi {
                write!(writer, "{GRAY}")?;
            }
            ctx.field_format().format_fields(writer.by_ref(), event)?;
            if self.ansi {
                write!(writer, "{}", self.reset_color())?;
            }
        }

        writeln!(writer)?;
        Ok(())
    }
}
