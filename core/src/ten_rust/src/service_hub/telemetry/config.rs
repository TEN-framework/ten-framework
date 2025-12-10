//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

//! Telemetry configuration structures
//!
//! This module defines the configuration structures for telemetry services,
//! supporting multiple exporters (Prometheus, OTLP, Console) with their
//! specific settings.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use crate::constants::METRICS;

/// Top-level telemetry configuration
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct TelemetryConfig {
    #[serde(default)]
    pub enabled: bool,

    #[serde(default)]
    pub metrics: Option<MetricsConfig>,
}

/// Metrics-specific configuration
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct MetricsConfig {
    #[serde(default = "default_true")]
    pub enabled: bool,

    #[serde(default)]
    pub exporter: Option<ExporterConfig>,
}

/// Exporter configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExporterConfig {
    #[serde(rename = "type")]
    pub exporter_type: ExporterType,

    #[serde(default)]
    pub prometheus: Option<PrometheusConfig>,

    #[serde(default)]
    pub otlp: Option<OtlpConfig>,
}

/// Exporter type
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum ExporterType {
    Prometheus,
    Otlp,
    Console,
}

/// Prometheus exporter configuration (Pull mode)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PrometheusConfig {
    /// HTTP server host (default: "0.0.0.0")
    #[serde(default = "default_prometheus_host")]
    pub host: String,

    /// HTTP server port (default: 49483)
    #[serde(default = "default_prometheus_port")]
    pub port: u16,

    /// Metrics endpoint path (default: "/metrics")
    #[serde(default = "default_prometheus_path")]
    pub path: String,
}

/// OTLP exporter configuration (Push mode)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OtlpConfig {
    /// OTLP endpoint URL
    pub endpoint: String,

    /// Protocol: "grpc" or "http" (default: "grpc")
    #[serde(default = "default_otlp_protocol")]
    pub protocol: OtlpProtocol,

    /// HTTP headers for authentication
    #[serde(default)]
    pub headers: HashMap<String, String>,
}

/// OTLP protocol type
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum OtlpProtocol {
    Grpc,
    Http,
}

// Default value functions
fn default_true() -> bool {
    true
}

fn default_prometheus_host() -> String {
    "0.0.0.0".to_string()
}

fn default_prometheus_port() -> u16 {
    49483
}

fn default_prometheus_path() -> String {
    METRICS.to_string()
}

fn default_otlp_protocol() -> OtlpProtocol {
    OtlpProtocol::Grpc
}

impl Default for ExporterConfig {
    fn default() -> Self {
        Self {
            exporter_type: ExporterType::Prometheus,
            prometheus: Some(PrometheusConfig::default()),
            otlp: None,
        }
    }
}

impl Default for PrometheusConfig {
    fn default() -> Self {
        Self {
            host: default_prometheus_host(),
            port: default_prometheus_port(),
            path: default_prometheus_path(),
        }
    }
}

impl TelemetryConfig {
    /// Parse from JSON value
    pub fn from_json(value: &serde_json::Value) -> Result<Self, serde_json::Error> {
        serde_json::from_value(value.clone())
    }

    /// Get the effective exporter type (with fallback logic)
    pub fn get_exporter_type(&self) -> ExporterType {
        self.metrics
            .as_ref()
            .and_then(|m| m.exporter.as_ref())
            .map(|e| e.exporter_type.clone())
            .unwrap_or(ExporterType::Prometheus)
    }

    /// Get Prometheus host and port (if applicable)
    pub fn get_prometheus_endpoint(&self) -> Option<String> {
        self.get_prometheus_config().map(|config| format!("{}:{}", config.host, config.port))
    }

    /// Get Prometheus metrics path (if applicable)
    pub fn get_prometheus_path(&self) -> Option<String> {
        self.get_prometheus_config().map(|config| config.path.clone())
    }

    /// Get Prometheus configuration (if applicable)
    pub fn get_prometheus_config(&self) -> Option<&PrometheusConfig> {
        self.metrics.as_ref().and_then(|m| m.exporter.as_ref()).and_then(|e| e.prometheus.as_ref())
    }

    /// Get OTLP configuration (if applicable)
    pub fn get_otlp_config(&self) -> Option<&OtlpConfig> {
        self.metrics.as_ref().and_then(|m| m.exporter.as_ref()).and_then(|e| e.otlp.as_ref())
    }

    /// Check if metrics are enabled
    pub fn is_metrics_enabled(&self) -> bool {
        self.enabled && self.metrics.as_ref().map(|m| m.enabled).unwrap_or(true)
    }
}
