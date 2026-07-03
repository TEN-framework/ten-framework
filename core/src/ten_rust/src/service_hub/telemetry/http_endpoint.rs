//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use actix_web::{web, HttpResponse};

/// Configure telemetry HTTP route for Prometheus scraping
///
/// This is only used when using Prometheus exporter (Pull mode).
/// For OTLP exporter (Push mode), this is not needed.
///
/// # Arguments
/// * `cfg` - The service configuration
/// * `registry` - The Prometheus registry
/// * `path` - The metrics endpoint path (e.g., "/metrics")
pub fn configure_metrics_endpoint(
    cfg: &mut web::ServiceConfig,
    registry: prometheus::Registry,
    path: String,
) {
    use prometheus::Encoder;

    cfg.route(
        &path,
        web::get().to(move || {
            let registry = registry.clone();
            async move {
                // Gather all metrics from the registry
                let metric_families = registry.gather();

                // Encode to Prometheus text format
                let encoder = prometheus::TextEncoder::new();
                let mut buffer = Vec::new();

                if encoder.encode(&metric_families, &mut buffer).is_err() {
                    return HttpResponse::InternalServerError().body("Failed to encode metrics");
                }

                // Return as plain text
                match String::from_utf8(buffer) {
                    Ok(response) => HttpResponse::Ok()
                        .content_type("text/plain; version=0.0.4; charset=utf-8")
                        .body(response),
                    Err(_) => HttpResponse::InternalServerError()
                        .body("Failed to convert metrics to UTF-8"),
                }
            }
        }),
    );
}
