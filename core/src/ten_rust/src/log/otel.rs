//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{cell::Cell, fmt};

use opentelemetry::KeyValue;
use opentelemetry_otlp::WithExportConfig;
use opentelemetry_sdk::{logs::SdkLoggerProvider, Resource};
use tracing::{level_filters::LevelFilter, Event, Subscriber};
use tracing_subscriber::{
    field::Visit, filter::Targets, layer::Context, registry::LookupSpan, Layer, Registry,
};

use super::{OtlpEmitterConfig, OtlpProtocol};

thread_local! {
    /// Prevent OTLP exporter internal tracing re-entry.
    ///
    /// When setting the handler level to DEBUG, libraries like opentelemetry/tonic/hyper
    /// may generate log events. These events can re-enter this layer, causing
    /// infinite recursion -> stack overflow.
    static IN_OTLP_LAYER: Cell<bool> = const { Cell::new(false) };
}

/// A guard to prevent re-entrant calls into the OTLP layer.
///
/// This uses a thread-local flag to detect if the current thread is already
/// processing an event within the OTLP layer. If so, it prevents further
/// processing of events originating from the OTLP exporter's internal logging,
/// avoiding infinite recursion and stack overflows.
///
/// It also handles potential `AccessError` during process shutdown by using `try_with`.
struct OtlpRecursionGuard;

impl OtlpRecursionGuard {
    /// Attempts to enter the OTLP layer.
    ///
    /// Returns `Some(OtlpRecursionGuard)` if successful (i.e., not already in the layer),
    /// otherwise returns `None` (if already in the layer or if TLS is unavailable).
    fn try_enter() -> Option<Self> {
        match IN_OTLP_LAYER.try_with(|flag| {
            let already_in_layer = flag.get();
            if !already_in_layer {
                flag.set(true);
            }
            !already_in_layer // Return true if we successfully entered, false if already in
        }) {
            Ok(true) => Some(OtlpRecursionGuard),
            _ => None, // Either already in layer, or TLS access failed
        }
    }
}

impl Drop for OtlpRecursionGuard {
    fn drop(&mut self) {
        // Reset the flag when the guard is dropped.
        // Use try_with to avoid TLS AccessError during process exit.
        let _ = IN_OTLP_LAYER.try_with(|flag| flag.set(false));
    }
}

/// Guard for OTLP telemetry resources
///
/// This guard holds the logger provider and runtime thread handle.
/// When dropped, it will shutdown the logger provider (flushing all buffered
/// logs)
pub struct OtlpTelemetryGuard {
    provider: Option<SdkLoggerProvider>,
    // Keep the runtime thread alive
    _runtime_handle: Option<std::thread::JoinHandle<()>>,
}

impl Drop for OtlpTelemetryGuard {
    fn drop(&mut self) {
        // Skip shutdown if we're panicking or if TLS might be destroyed
        if std::thread::panicking() {
            return;
        }

        if let Some(provider) = self.provider.take() {
            // Use eprintln! instead of tracing macros to avoid TLS access during shutdown
            eprintln!("[OTLP] Shutting down OpenTelemetry logger provider...");

            // Attempt to shutdown, but don't panic if it fails due to TLS issues
            // This can happen during process exit when TLS is being destroyed
            let shutdown_result =
                std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| provider.shutdown()));

            match shutdown_result {
                Ok(Ok(())) => {
                    eprintln!("[OTLP] Logger provider shut down successfully");
                }
                Ok(Err(e)) => {
                    eprintln!("[OTLP] Failed to shutdown logger provider: {:?}", e);
                }
                Err(_) => {
                    eprintln!(
                        "[OTLP] Logger provider shutdown panicked (likely due to TLS destruction \
                         during process exit)"
                    );
                }
            }
        }
    }
}

/// Visitor that collects all fields from a tracing event
#[derive(Default)]
struct FieldCollector {
    fields: Vec<(String, String)>,
    user_fields_json: Option<String>,
    message: String,
}

impl Visit for FieldCollector {
    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn fmt::Debug) {
        let field_name = field.name();
        let value_str = format!("{value:?}");

        if field_name == "ten_user_fields" {
            self.user_fields_json = Some(value_str.trim_matches('"').to_string());
        } else if field_name == "message" {
            self.message = value_str.trim_matches('"').to_string();
        } else {
            self.fields.push((field_name.to_string(), value_str));
        }
    }

    fn record_str(&mut self, field: &tracing::field::Field, value: &str) {
        let field_name = field.name();

        if field_name == "ten_user_fields" {
            self.user_fields_json = Some(value.to_string());
        } else if field_name == "message" {
            self.message = value.to_string();
        } else {
            self.fields.push((field_name.to_string(), value.to_string()));
        }
    }

    fn record_i64(&mut self, field: &tracing::field::Field, value: i64) {
        self.fields.push((field.name().to_string(), value.to_string()));
    }

    fn record_u64(&mut self, field: &tracing::field::Field, value: u64) {
        self.fields.push((field.name().to_string(), value.to_string()));
    }

    fn record_f64(&mut self, field: &tracing::field::Field, value: f64) {
        self.fields.push((field.name().to_string(), value.to_string()));
    }

    fn record_bool(&mut self, field: &tracing::field::Field, value: bool) {
        self.fields.push((field.name().to_string(), value.to_string()));
    }
}

/// Custom OpenTelemetry Layer that expands ten_user_fields into individual
/// attributes
///
/// This layer intercepts tracing events, extracts and parses ten_user_fields
/// JSON, then emits OpenTelemetry log records with all fields (including
/// expanded user fields) as individual attributes.
pub struct TenOtelLayer {
    provider: SdkLoggerProvider,
}

impl TenOtelLayer {
    pub fn new(provider: SdkLoggerProvider) -> Self {
        Self {
            provider,
        }
    }

    /// Parse user_fields JSON string and extract key-value pairs
    fn parse_user_fields(&self, json_str: &str) -> Vec<(String, opentelemetry::logs::AnyValue)> {
        if json_str.is_empty() {
            return Vec::new();
        }

        // Try to parse as JSON object
        match serde_json::from_str::<serde_json::Value>(json_str) {
            Ok(serde_json::Value::Object(obj)) => obj
                .into_iter()
                .map(|(k, v)| {
                    // Convert JSON values to OpenTelemetry AnyValue
                    let otel_value = match &v {
                        serde_json::Value::String(s) => {
                            opentelemetry::logs::AnyValue::from(s.clone())
                        }
                        serde_json::Value::Number(n) => {
                            if let Some(i) = n.as_i64() {
                                opentelemetry::logs::AnyValue::from(i)
                            } else if let Some(f) = n.as_f64() {
                                opentelemetry::logs::AnyValue::from(f)
                            } else {
                                opentelemetry::logs::AnyValue::from(v.to_string())
                            }
                        }
                        serde_json::Value::Bool(b) => opentelemetry::logs::AnyValue::from(*b),
                        serde_json::Value::Null => opentelemetry::logs::AnyValue::from("null"),
                        serde_json::Value::Array(_) | serde_json::Value::Object(_) => {
                            // For complex types, serialize to JSON string
                            opentelemetry::logs::AnyValue::from(v.to_string())
                        }
                    };
                    (k, otel_value)
                })
                .collect(),
            _ => Vec::new(),
        }
    }
}

impl<S> Layer<S> for TenOtelLayer
where
    S: Subscriber + for<'a> LookupSpan<'a>,
{
    fn on_event(&self, event: &Event<'_>, _ctx: Context<'_, S>) {
        use opentelemetry::logs::{LogRecord, Logger, LoggerProvider, Severity};

        // Thread-local re-entrancy protection:
        // If we detect recursion (logging triggered by the exporter itself), skip this event.
        // This prevents stack overflow when debug logging is enabled for the exporter's dependencies.
        let _guard = match OtlpRecursionGuard::try_enter() {
            Some(g) => g,
            None => return,
        };


        let metadata = event.metadata();
        let logger = self.provider.logger("ten-framework");

        // Collect all fields from the event
        let mut collector = FieldCollector::default();
        event.record(&mut collector);

        // Create OpenTelemetry log record
        let mut log_record = logger.create_log_record();

        // Set severity based on tracing level
        let severity = match *metadata.level() {
            tracing::Level::TRACE => Severity::Trace,
            tracing::Level::DEBUG => Severity::Debug,
            tracing::Level::INFO => Severity::Info,
            tracing::Level::WARN => Severity::Warn,
            tracing::Level::ERROR => Severity::Error,
        };
        log_record.set_severity_number(severity);
        log_record.set_severity_text(metadata.level().as_str());

        // Set the log body (message)
        if !collector.message.is_empty() {
            log_record.set_body(collector.message.into());
        }

        // Add target as an attribute
        log_record.add_attribute("target", opentelemetry::logs::AnyValue::from(metadata.target()));

        // Add all standard fields as attributes
        for (key, value) in collector.fields {
            // Parse numeric values if possible
            let otel_value = if let Ok(i) = value.parse::<i64>() {
                opentelemetry::logs::AnyValue::from(i)
            } else if let Ok(f) = value.parse::<f64>() {
                opentelemetry::logs::AnyValue::from(f)
            } else if value == "true" || value == "false" {
                opentelemetry::logs::AnyValue::from(value == "true")
            } else {
                // Remove surrounding quotes if present
                let cleaned = value.trim_matches('"').to_string();
                opentelemetry::logs::AnyValue::from(cleaned)
            };
            log_record.add_attribute(key, otel_value);
        }

        // Parse and expand user_fields into individual attributes
        if let Some(user_fields_json) = collector.user_fields_json {
            let expanded_fields = self.parse_user_fields(&user_fields_json);
            for (key, value) in expanded_fields {
                log_record.add_attribute(key, value);
            }
        }

        // Emit the log record
        logger.emit(log_record);
    }
}

pub fn create_otlp_layer(
    config: &OtlpEmitterConfig,
) -> (Box<dyn Layer<Registry> + Send + Sync>, OtlpTelemetryGuard) {
    let service_name = config.service_name.clone().unwrap_or_else(|| "ten-framework".to_string());
    let endpoint = config.endpoint.clone();
    let protocol = config.protocol.clone();
    let _headers = config.headers.clone(); // TODO: Add header support when API available

    eprintln!(
        "[OTLP] Initializing OTLP log layer: endpoint={}, service_name={}, protocol={:?}",
        endpoint, service_name, protocol
    );

    // Channel to receive the LoggerProvider
    let (tx, rx) = std::sync::mpsc::channel();
    let endpoint_for_thread = endpoint.clone();

    let handle = std::thread::spawn(move || {
        let rt = tokio::runtime::Runtime::new().expect("Failed to create Tokio runtime");
        rt.block_on(async {
            let resource = Resource::builder()
                .with_service_name(service_name)
                .with_attributes(vec![KeyValue::new("service.namespace", "ten-framework")])
                .build();

            // Setup OTLP log exporter based on protocol
            eprintln!("[OTLP] Creating log exporter for endpoint: {}", endpoint_for_thread);
            let exporter_result = match protocol {
                OtlpProtocol::Grpc => {
                    eprintln!("[OTLP] Using gRPC protocol");
                    opentelemetry_otlp::LogExporter::builder()
                        .with_tonic()
                        .with_endpoint(&endpoint_for_thread)
                        .build()
                }
                OtlpProtocol::Http => {
                    eprintln!("[OTLP] Using HTTP protocol");
                    opentelemetry_otlp::LogExporter::builder()
                        .with_http()
                        .with_endpoint(&endpoint_for_thread)
                        .build()
                }
            };

            let exporter = match exporter_result {
                Ok(exp) => {
                    eprintln!("[OTLP] Log exporter created successfully");
                    exp
                }
                Err(e) => {
                    eprintln!("[OTLP] FAILED to create OTLP log exporter!");
                    eprintln!("[OTLP] Error: {:?}", e);
                    eprintln!("[OTLP] Endpoint: {}", endpoint_for_thread);
                    eprintln!("[OTLP] Protocol: {:?}", protocol);
                    eprintln!("[OTLP] Logs will NOT be exported to OTLP endpoint!");
                    return;
                }
            };

            // Setup Logger Provider
            eprintln!("[OTLP] Creating logger provider with batch exporter...");
            let provider = SdkLoggerProvider::builder()
                .with_batch_exporter(exporter)
                .with_resource(resource)
                .build();

            eprintln!("[OTLP] Logger provider created successfully");

            // Send provider back to main thread
            if tx.send(provider).is_err() {
                eprintln!("[OTLP] Failed to send logger provider to main thread");
            }

            // Keep the runtime alive
            std::future::pending::<()>().await;
        });
    });

    // Wait for logger provider
    let provider = rx.recv().expect("Failed to receive logger provider");

    // Create our custom TEN OpenTelemetry layer
    let layer = TenOtelLayer::new(provider.clone());

    // Prevent infinite recursion by filtering out logs from the OTLP exporter's
    // underlying libraries. This uses a declarative filter wrapper which is
    // more efficient and semantically correct than checking targets manually
    // inside the layer. We default to TRACE specifically for this filter to
    // allow the parent `DynamicTargetFilterLayer` to control the actual logging
    // level based on user config.
    let recursion_filter = Targets::new()
        .with_target("opentelemetry", LevelFilter::OFF)
        .with_target("tonic", LevelFilter::OFF)
        .with_target("h2", LevelFilter::OFF)
        .with_target("hyper", LevelFilter::OFF)
        .with_target("tower", LevelFilter::OFF)
        .with_target("reqwest", LevelFilter::OFF)
        .with_target("rustls", LevelFilter::OFF)
        .with_default(LevelFilter::TRACE);

    let layer = layer.with_filter(recursion_filter);

    eprintln!("[OTLP] OTLP log layer created and ready");
    eprintln!("[OTLP] User fields will be expanded into individual attributes");
    eprintln!("[OTLP] Note: If you see 'BatchLogProcessor.ExportError', check:");
    eprintln!("[OTLP]   1. Is the OTLP collector running at {}?", endpoint);
    eprintln!("[OTLP]   2. Is the endpoint URL correct?");
    eprintln!("[OTLP]   3. Check network connectivity and firewall rules");

    let guard = OtlpTelemetryGuard {
        provider: Some(provider),
        _runtime_handle: Some(handle),
    };

    (Box::new(layer), guard)
}
