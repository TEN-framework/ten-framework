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

use super::config::{OtlpProtocol, TelemetryConfig};

/// Metrics exporter type
#[derive(Debug, Clone)]
pub enum ExporterType {
    /// Prometheus exporter (Pull mode, /metrics endpoint)
    Prometheus,

    /// OTLP exporter (Push mode, to Collector/Langfuse/etc)
    Otlp {
        endpoint: String,
        protocol: OtlpProtocol,
        headers: std::collections::HashMap<String, String>,
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
    ///   "_ten": {
    ///     "services": {
    ///       "telemetry": {
    ///         "enabled": true,
    ///         "host": "0.0.0.0",
    ///         "port": 49483,
    ///         "metrics": {
    ///           "enabled": true,
    ///           "exporter": {
    ///             "type": "prometheus",
    ///             "prometheus": {
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
    ///   "_ten": {
    ///     "services": {
    ///       "telemetry": {
    ///         "enabled": true,
    ///         "host": "0.0.0.0",
    ///         "port": 49483,
    ///         "metrics": {
    ///           "enabled": true,
    ///           "exporter": {
    ///             "type": "otlp",
    ///             "otlp": {
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
    pub fn from_config(config: &TelemetryConfig) -> Self {
        use super::config::ExporterType as ConfigExporterType;

        let exporter_type = config.get_exporter_type();

        match exporter_type {
            ConfigExporterType::Prometheus => {
                tracing::info!("📊 Telemetry: Using Prometheus exporter (Pull mode)");
                ExporterType::Prometheus
            }
            ConfigExporterType::Otlp => {
                if let Some(otlp_config) = config.get_otlp_config() {
                    tracing::info!("📊 Telemetry: Using OTLP exporter (Push mode)");
                    tracing::info!("   Endpoint: {}", otlp_config.endpoint);
                    tracing::info!("   Protocol: {:?}", otlp_config.protocol);
                    if !otlp_config.headers.is_empty() {
                        tracing::info!("   Headers: {} configured", otlp_config.headers.len());
                    }

                    ExporterType::Otlp {
                        endpoint: otlp_config.endpoint.clone(),
                        protocol: otlp_config.protocol.clone(),
                        headers: otlp_config.headers.clone(),
                    }
                } else {
                    tracing::warn!(
                        "⚠️  Warning: OTLP exporter selected but no config provided, falling back \
                         to Prometheus"
                    );
                    ExporterType::Prometheus
                }
            }
            ConfigExporterType::Console => {
                tracing::info!("📊 Telemetry: Using Console exporter (Debug mode)");
                ExporterType::Console
            }
        }
    }
}

/// Metrics exporter service
pub struct MetricsExporter {
    meter_provider: Arc<Mutex<Option<SdkMeterProvider>>>,
    exporter_type: ExporterType,

    // Only used for Prometheus exporter
    prometheus_registry: Arc<Mutex<Option<prometheus::Registry>>>,
}

impl MetricsExporter {
    pub fn new(exporter_type: ExporterType) -> Self {
        Self {
            meter_provider: Arc::new(Mutex::new(None)),
            exporter_type,
            prometheus_registry: Arc::new(Mutex::new(None)),
        }
    }

    /// Initialize the exporter with given service name
    pub fn init(&self, service_name: &str) -> Result<()> {
        match &self.exporter_type {
            ExporterType::Prometheus => self.init_prometheus_exporter(service_name),
            ExporterType::Otlp {
                endpoint,
                protocol,
                headers,
            } => self.init_otlp_exporter(service_name, endpoint, protocol, headers),
            ExporterType::Console => self.init_console_exporter(service_name),
        }
    }

    /// Initialize with Prometheus exporter (Pull mode)
    fn init_prometheus_exporter(&self, service_name: &str) -> Result<()> {
        let resource = Self::create_resource(service_name);

        // Create Prometheus registry and exporter
        let registry = prometheus::Registry::new();
        let exporter =
            opentelemetry_prometheus::exporter().with_registry(registry.clone()).build()?;

        // Create meter provider
        let provider =
            SdkMeterProvider::builder().with_reader(exporter).with_resource(resource).build();

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
        let resource = Self::create_resource(service_name);

        // TODO: Implement OTLP exporter
        // This will be used for pushing to Collector/Langfuse/Datadog/etc

        tracing::warn!("OTLP exporter not yet implemented");
        tracing::info!("  endpoint: {}", endpoint);
        tracing::info!("  protocol: {:?}", protocol);
        tracing::info!("  headers: {:?}", headers);

        // Placeholder
        let provider = SdkMeterProvider::builder().with_resource(resource).build();

        opentelemetry::global::set_meter_provider(provider.clone());
        *self.meter_provider.lock().unwrap() = Some(provider);

        Ok(())
    }

    /// Initialize with Console exporter (for debugging)
    fn init_console_exporter(&self, service_name: &str) -> Result<()> {
        let resource = Self::create_resource(service_name);

        tracing::info!("🖥️  Console exporter: Metrics will be printed to stdout");
        tracing::info!("   Service: {}", service_name);

        // Create stdout exporter
        let exporter = opentelemetry_stdout::MetricExporterBuilder::default().build();

        // Create periodic reader to export metrics every 30 seconds
        let reader = opentelemetry_sdk::metrics::PeriodicReader::builder(exporter)
            .with_interval(std::time::Duration::from_secs(30))
            .build();

        // Create meter provider
        let provider =
            SdkMeterProvider::builder().with_reader(reader).with_resource(resource).build();

        // Set global meter provider
        opentelemetry::global::set_meter_provider(provider.clone());

        // Store provider
        *self.meter_provider.lock().unwrap() = Some(provider);

        tracing::info!("✅ Console exporter initialized (export interval: 30s)");

        Ok(())
    }

    /// Create OpenTelemetry Resource with service metadata
    fn create_resource(service_name: &str) -> Resource {
        // Use builder pattern which is public API
        Resource::builder()
            .with_service_name(service_name.to_string())
            .with_attributes(vec![
                KeyValue::new("service.namespace", "ten-framework"),
            ])
            .build()
    }

    /// Get Prometheus registry (only available for Prometheus exporter)
    pub fn get_prometheus_registry(&self) -> Option<prometheus::Registry> {
        self.prometheus_registry.lock().unwrap().clone()
    }

    /// Shutdown the exporter
    pub fn shutdown(&self) -> Result<()> {
        if let Some(provider) = self.meter_provider.lock().unwrap().take() {
            provider.shutdown()?;
        }
        Ok(())
    }
}

impl Default for MetricsExporter {
    fn default() -> Self {
        Self::new(ExporterType::Prometheus)
    }
}
