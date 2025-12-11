//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

//! # TEN Framework Telemetry Module
//!
//! This module provides OpenTelemetry-based observability for TEN Framework.
//!
//! ## Architecture
//!
//! ```text
//! TEN Framework
//!     │
//!     └──► OpenTelemetry SDK
//!            │
//!            ├──► Metrics (this module)
//!            │      ├──► Prometheus Exporter (Pull mode)
//!            │      ├──► OTLP Exporter (Push mode) [Future]
//!            │      └──► Custom Exporters [Future]
//!            │
//!            ├──► Traces [Future]
//!            └──► Logs [Future]
//! ```
//!
//! ## Modules
//!
//! - `metrics`: Core metrics functionality (backend-agnostic)
//! - `exporter`: Exporter implementations (Prometheus, OTLP, etc)
//! - `http_endpoint`: HTTP endpoint for Prometheus Pull mode
//!
//! ## Usage
//!
//! ### From Rust:
//!
//! ```rust,ignore
//! use service_hub::telemetry::{record_counter, record_histogram};
//!
//! record_counter("requests_total", 1, &[("method", "GET")]);
//! record_histogram("request_duration_ms", 123.5, &[]);
//! ```
//!
//! ### From C:
//!
//! ```c
//! MetricHandle *counter = ten_metric_create(NULL, 0, "requests", "help", NULL, 0);
//! ten_metric_counter_inc(counter, NULL, 0);
//! ten_metric_destroy(counter);
//! ```

pub mod config;
pub mod exporter;
pub mod http_endpoint;
pub mod metrics;

// Re-export commonly used items
pub use config::{
    ExporterConfig, MetricsConfig, OtlpConfig, OtlpProtocol, PrometheusConfig, TelemetryConfig,
};
pub use exporter::{ExporterType, MetricsExporter};
pub use http_endpoint::configure_metrics_endpoint;
// Allow unused imports for public API exports
#[allow(unused_imports)]
pub use metrics::{record_counter, record_gauge, record_histogram, MetricHandle, MetricType};
// Re-export C FFI functions (already exported by metrics module)
#[allow(unused_imports)]
pub use metrics::{
    ten_metric_counter_add, ten_metric_counter_inc, ten_metric_create, ten_metric_destroy,
    ten_metric_gauge_add, ten_metric_gauge_dec, ten_metric_gauge_inc, ten_metric_gauge_set,
    ten_metric_gauge_sub, ten_metric_histogram_observe,
};
