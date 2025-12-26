//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use opentelemetry::KeyValue;
use opentelemetry_appender_tracing::layer::OpenTelemetryTracingBridge;
use opentelemetry_otlp::WithExportConfig;
use opentelemetry_sdk::{logs::SdkLoggerProvider, Resource};
use tracing_subscriber::{Layer, Registry};

use super::{OtlpEmitterConfig, OtlpProtocol};

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

    // Create the OpenTelemetry tracing bridge layer
    let layer = OpenTelemetryTracingBridge::new(&provider);

    eprintln!("[OTLP] OTLP log layer created and ready");
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
