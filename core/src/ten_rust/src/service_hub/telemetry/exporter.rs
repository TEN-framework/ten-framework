//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::sync::{Arc, Mutex};

use anyhow::Result;
use opentelemetry::KeyValue;
use opentelemetry_sdk::{metrics::SdkMeterProvider, Resource};

use super::{
    config::{OtlpProtocol, TelemetryConfig},
    runtime_stats::RuntimeMetricsReader,
};

/// Metrics exporter type
#[derive(Debug, Clone)]
pub enum ExporterType {
    /// Collect metrics locally without an external exporter.
    None { service_name: Option<String> },

    /// Prometheus exporter (Pull mode, /metrics endpoint)
    Prometheus { service_name: Option<String> },

    /// OTLP exporter (Push mode, to Collector/Langfuse/etc)
    Otlp {
        endpoint: String,
        protocol: OtlpProtocol,
        headers: std::collections::HashMap<String, String>,
        service_name: Option<String>,
    },

    /// Console exporter (for debugging)
    Console,
}

impl ExporterType {
    /// Create ExporterType from TelemetryConfig
    ///
    /// # Example configuration (property.json):
    ///
    /// ```json
    /// {
    ///   "ten": {
    ///     "services": {
    ///       "telemetry": {
    ///         "enabled": true,
    ///         "metrics": {
    ///           "enabled": true,
    ///           "exporter": {
    ///             "type": "prometheus",
    ///             "config": {
    ///               "endpoint": "0.0.0.0:49483",
    ///               "path": "/metrics"
    ///             }
    ///           }
    ///         }
    ///       }
    ///     }
    ///   }
    /// }
    /// ```
    ///
    /// For OTLP exporter (to send to OpenTelemetry Collector, Langfuse, etc):
    ///
    /// ```json
    /// {
    ///   "ten": {
    ///     "services": {
    ///       "telemetry": {
    ///         "enabled": true,
    ///         "metrics": {
    ///           "enabled": true,
    ///           "exporter": {
    ///             "type": "otlp",
    ///             "config": {
    ///               "endpoint": "http://localhost:4317",
    ///               "protocol": "grpc",
    ///               "headers": {
    ///                 "x-api-key": "your-api-key"
    ///               }
    ///             }
    ///           }
    ///         }
    ///       }
    ///     }
    ///   }
    /// }
    /// ```
    ///
    /// For Console exporter (debugging):
    ///
    /// ```json
    /// {
    ///   "ten": {
    ///     "services": {
    ///       "telemetry": {
    ///         "enabled": true,
    ///         "metrics": {
    ///           "enabled": true,
    ///           "exporter": {
    ///             "type": "console",
    ///             "config": {}
    ///           }
    ///         }
    ///       }
    ///     }
    ///   }
    /// }
    /// ```
    pub fn from_config(config: &TelemetryConfig) -> Self {
        use super::config::ExporterType as ConfigExporterType;

        let exporter_type = config.get_exporter_type();

        match exporter_type {
            ConfigExporterType::Prometheus => {
                tracing::info!("Telemetry: Using Prometheus exporter (Pull mode)");
                ExporterType::Prometheus {
                    service_name: config
                        .get_prometheus_config()
                        .and_then(|c| c.service_name.clone()),
                }
            }
            ConfigExporterType::Otlp => {
                if let Some(otlp_config) = config.get_otlp_config() {
                    if let Some(endpoint) = otlp_config.resolve_endpoint() {
                        tracing::info!("Telemetry: Using OTLP exporter (Push mode)");
                        ExporterType::Otlp {
                            endpoint,
                            protocol: otlp_config.protocol.clone(),
                            headers: otlp_config.headers.clone(),
                            service_name: otlp_config.service_name.clone(),
                        }
                    } else {
                        tracing::warn!(
                            "OTLP metrics endpoint is missing or invalid; collecting metrics \
                             locally without external export"
                        );
                        ExporterType::None {
                            service_name: otlp_config.service_name.clone(),
                        }
                    }
                } else {
                    tracing::warn!(
                        "OTLP exporter selected without configuration; collecting metrics locally"
                    );
                    ExporterType::None {
                        service_name: None,
                    }
                }
            }
            ConfigExporterType::Console => {
                tracing::info!("Telemetry: Using Console exporter (Debug mode)");
                ExporterType::Console
            }
        }
    }
}

/// Metrics exporter service
pub struct MetricsExporter {
    meter_provider: Arc<Mutex<Option<SdkMeterProvider>>>,
    exporter_type: ExporterType,
    runtime_metrics_reader: RuntimeMetricsReader,

    // Only used for Prometheus exporter
    prometheus_registry: Arc<Mutex<Option<prometheus::Registry>>>,

    // Runtime handle for OTLP exporter (kept alive for the entire lifecycle)
    _runtime_handle: Arc<Mutex<Option<std::thread::JoinHandle<()>>>>,
    runtime_shutdown_tx: Mutex<Option<std::sync::mpsc::Sender<()>>>,
}

impl MetricsExporter {
    pub fn new(exporter_type: ExporterType) -> Self {
        Self {
            meter_provider: Arc::new(Mutex::new(None)),
            exporter_type,
            runtime_metrics_reader: RuntimeMetricsReader::default(),
            prometheus_registry: Arc::new(Mutex::new(None)),
            _runtime_handle: Arc::new(Mutex::new(None)),
            runtime_shutdown_tx: Mutex::new(None),
        }
    }

    /// Initialize the exporter with given service name
    pub fn init(&self, default_service_name: &str) -> Result<()> {
        match &self.exporter_type {
            ExporterType::None {
                service_name,
            } => self.init_local_metrics(service_name.as_deref().unwrap_or(default_service_name)),
            ExporterType::Prometheus {
                service_name,
            } => {
                let effective_service_name =
                    service_name.as_deref().unwrap_or(default_service_name);
                self.init_prometheus_exporter(effective_service_name)
            }
            ExporterType::Otlp {
                endpoint,
                protocol,
                headers,
                service_name,
            } => {
                let effective_service_name =
                    service_name.as_deref().unwrap_or(default_service_name);
                if let Err(error) =
                    self.init_otlp_exporter(effective_service_name, endpoint, protocol, headers)
                {
                    tracing::warn!(
                        "OTLP metrics initialization failed: {error}; collecting metrics locally \
                         without external export"
                    );
                    self.init_local_metrics(effective_service_name)?;
                }
                Ok(())
            }
            ExporterType::Console => self.init_console_exporter(default_service_name),
        }
    }

    /// Keep SDK aggregation available to ten:runtime_stats without exporting.
    fn init_local_metrics(&self, service_name: &str) -> Result<()> {
        let provider = SdkMeterProvider::builder()
            .with_reader(self.runtime_metrics_reader.clone())
            .with_resource(Self::create_resource(service_name))
            .build();
        opentelemetry::global::set_meter_provider(provider.clone());
        *self.meter_provider.lock().unwrap() = Some(provider);
        Ok(())
    }

    /// Initialize with Prometheus exporter (Pull mode)
    fn init_prometheus_exporter(&self, service_name: &str) -> Result<()> {
        let resource = Self::create_resource(service_name);

        // Create Prometheus registry and exporter
        let registry = prometheus::Registry::new();
        let exporter =
            opentelemetry_prometheus::exporter().with_registry(registry.clone()).build()?;

        // Create meter provider
        let provider = SdkMeterProvider::builder()
            .with_reader(exporter)
            .with_reader(self.runtime_metrics_reader.clone())
            .with_resource(resource)
            .build();

        // Set global meter provider
        opentelemetry::global::set_meter_provider(provider.clone());

        // Store provider and registry
        *self.meter_provider.lock().unwrap() = Some(provider);
        *self.prometheus_registry.lock().unwrap() = Some(registry);

        Ok(())
    }

    /// Initialize with OTLP exporter (Push mode)
    fn init_otlp_exporter(
        &self,
        service_name: &str,
        endpoint: &str,
        protocol: &OtlpProtocol,
        headers: &std::collections::HashMap<String, String>,
    ) -> Result<()> {
        tracing::info!("Telemetry: Pushing metrics to collector");
        tracing::info!("  Service: {}", service_name);
        tracing::info!("  Endpoint: {}", endpoint);
        tracing::info!("  Protocol: {:?}", protocol);
        if !headers.is_empty() {
            tracing::info!("  Headers: {} configured", headers.len());
        }

        // Create a dedicated runtime thread for OTLP exporter
        // This runtime will stay alive for the entire application lifecycle
        let endpoint_clone = endpoint.to_string();
        let protocol_clone = protocol.clone();
        let (init_tx, init_rx) =
            std::sync::mpsc::channel::<Result<opentelemetry_otlp::MetricExporter>>();
        let (shutdown_tx, shutdown_rx) = std::sync::mpsc::channel::<()>();

        let runtime_handle =
            std::thread::Builder::new().name("otlp-metrics".into()).spawn(move || {
                tracing::debug!("Starting dedicated OTLP runtime thread");

                // Create a new Tokio runtime for this thread
                let rt = match tokio::runtime::Runtime::new() {
                    Ok(rt) => rt,
                    Err(e) => {
                        let _ = init_tx
                            .send(Err(anyhow::anyhow!("Failed to create Tokio runtime: {}", e)));
                        return;
                    }
                };

                use opentelemetry_otlp::WithExportConfig;
                let exporter = match &protocol_clone {
                    OtlpProtocol::Grpc => {
                        // Tonic needs a running Tokio runtime for its lazy
                        // channel.
                        let _guard = rt.enter();
                        opentelemetry_otlp::MetricExporter::builder()
                            .with_tonic()
                            .with_endpoint(&endpoint_clone)
                            .build()
                    }
                    OtlpProtocol::Http => {
                        // The blocking HTTP client must be built outside Tokio.
                        opentelemetry_otlp::MetricExporter::builder()
                            .with_http()
                            .with_endpoint(&endpoint_clone)
                            .build()
                    }
                };
                match exporter {
                    Ok(exporter) => {
                        if init_tx.send(Ok(exporter)).is_ok() {
                            // Dropping the sender after timeout also ends this
                            // thread. It must never publish a late meter
                            // provider.
                            let _ = shutdown_rx.recv();
                        }
                    }
                    Err(error) => {
                        let _ = init_tx
                            .send(Err(anyhow::anyhow!("Failed to build OTLP exporter: {error}")));
                    }
                }
            })?;

        // Wait for initialization to complete (with timeout)
        match init_rx.recv_timeout(std::time::Duration::from_secs(5)) {
            Ok(Ok(exporter)) => {
                let reader = opentelemetry_sdk::metrics::PeriodicReader::builder(exporter)
                    .with_interval(std::time::Duration::from_secs(10))
                    .build();
                let provider = SdkMeterProvider::builder()
                    .with_reader(reader)
                    .with_reader(self.runtime_metrics_reader.clone())
                    .with_resource(Self::create_resource(service_name))
                    .build();
                opentelemetry::global::set_meter_provider(provider.clone());
                *self.meter_provider.lock().unwrap() = Some(provider);

                tracing::info!("OTLP exporter initialization confirmed");
                *self.runtime_shutdown_tx.lock().unwrap() = Some(shutdown_tx);
                *self._runtime_handle.lock().unwrap() = Some(runtime_handle);
                Ok(())
            }
            Ok(Err(e)) => {
                tracing::error!("OTLP exporter initialization failed: {}", e);
                Err(e)
            }
            Err(error) => {
                Err(anyhow::anyhow!("OTLP exporter initialization did not complete: {error}"))
            }
        }
    }

    /// Initialize with Console exporter (for debugging)
    fn init_console_exporter(&self, service_name: &str) -> Result<()> {
        let resource = Self::create_resource(service_name);

        tracing::info!("Console exporter: Metrics will be printed to stdout");
        tracing::info!("  Service: {}", service_name);

        // Create stdout exporter
        let exporter = opentelemetry_stdout::MetricExporterBuilder::default().build();

        // Create periodic reader to export metrics every 30 seconds
        let reader = opentelemetry_sdk::metrics::PeriodicReader::builder(exporter)
            .with_interval(std::time::Duration::from_secs(30))
            .build();

        // Create meter provider
        let provider = SdkMeterProvider::builder()
            .with_reader(reader)
            .with_reader(self.runtime_metrics_reader.clone())
            .with_resource(resource)
            .build();

        // Set global meter provider
        opentelemetry::global::set_meter_provider(provider.clone());

        // Store provider
        *self.meter_provider.lock().unwrap() = Some(provider);

        tracing::info!("Console exporter initialized (export interval: 30s)");

        Ok(())
    }

    /// Create OpenTelemetry Resource with service metadata
    fn create_resource(service_name: &str) -> Resource {
        // Use builder pattern which is public API
        Resource::builder()
            .with_service_name(service_name.to_string())
            .with_attributes(vec![KeyValue::new("service.namespace", "ten-framework")])
            .build()
    }

    /// Get Prometheus registry (only available for Prometheus exporter)
    pub fn get_prometheus_registry(&self) -> Option<prometheus::Registry> {
        self.prometheus_registry.lock().unwrap().clone()
    }

    /// Collect all scopes as cumulative metrics using the OTLP JSON model.
    pub fn collect_metrics(&self) -> Result<serde_json::Value> {
        self.runtime_metrics_reader.collect_json()
    }

    /// Shutdown the exporter
    pub fn shutdown(&self) -> Result<()> {
        let result = match self.meter_provider.lock().unwrap().take() {
            Some(provider) => provider.shutdown().map_err(Into::into),
            None => Ok(()),
        };
        self.runtime_shutdown_tx.lock().unwrap().take();
        if let Some(handle) = self._runtime_handle.lock().unwrap().take() {
            let _ = handle.join();
        }
        result
    }
}

impl Default for MetricsExporter {
    fn default() -> Self {
        Self::new(ExporterType::Prometheus {
            service_name: None,
        })
    }
}
