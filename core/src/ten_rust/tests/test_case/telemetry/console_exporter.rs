//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::{thread, time::Duration};

use ten_rust::service_hub::telemetry::{ExporterType, MetricsExporter};

#[test]
fn test_console_exporter_initialization() {
    println!("\n🧪 Testing Console Exporter Initialization\n");

    // Create console exporter
    let exporter = MetricsExporter::new(ExporterType::Console);

    // Initialize
    println!("📝 Initializing console exporter...");
    let result = exporter.init("test-service");

    assert!(result.is_ok(), "Failed to initialize console exporter: {:?}", result.err());
    println!("✅ Console exporter initialized successfully\n");

    // Test that metrics can be recorded
    println!("📊 Recording test metrics...\n");

    use opentelemetry::global;

    let meter = global::meter("test-meter");

    // Create and increment a counter
    let counter = meter.u64_counter("test_counter").build();
    counter.add(10, &[opentelemetry::KeyValue::new("env", "test")]);
    println!("   ✓ Counter: test_counter = 10 (env=test)");

    counter.add(5, &[opentelemetry::KeyValue::new("env", "test")]);
    println!("   ✓ Counter: test_counter += 5 (env=test)");

    // Create and set a gauge
    let gauge = meter.f64_gauge("test_gauge").build();
    gauge.record(42.5, &[opentelemetry::KeyValue::new("env", "test")]);
    println!("   ✓ Gauge: test_gauge = 42.5 (env=test)");

    gauge.record(100.0, &[opentelemetry::KeyValue::new("env", "test")]);
    println!("   ✓ Gauge: test_gauge = 100.0 (env=test)");

    // Create and record histogram
    let histogram = meter.f64_histogram("test_histogram").build();
    histogram.record(0.1, &[opentelemetry::KeyValue::new("env", "test")]);
    println!("   ✓ Histogram: test_histogram = 0.1 (env=test)");

    histogram.record(0.5, &[opentelemetry::KeyValue::new("env", "test")]);
    println!("   ✓ Histogram: test_histogram = 0.5 (env=test)");

    histogram.record(1.0, &[opentelemetry::KeyValue::new("env", "test")]);
    println!("   ✓ Histogram: test_histogram = 1.0 (env=test)\n");

    println!("ℹ️  Note: Metrics are recorded but won't be exported immediately");
    println!("   (Console exporter exports every 30 seconds)\n");

    // Shutdown
    println!("🛑 Shutting down exporter...");
    let result = exporter.shutdown();
    assert!(result.is_ok(), "Failed to shutdown exporter: {:?}", result.err());
    println!("✅ Exporter shut down successfully\n");
}

#[test]
fn test_console_exporter_with_wait() {
    println!("\n🧪 Testing Console Exporter with Export Wait\n");
    println!("⚠️  This test will wait 35 seconds for metrics export\n");

    // Create and initialize console exporter
    let exporter = MetricsExporter::new(ExporterType::Console);
    exporter.init("wait-test-service").expect("Failed to init");
    println!("✅ Console exporter initialized\n");

    // Record various metrics
    println!("📊 Recording metrics...\n");

    use opentelemetry::global;
    let meter = global::meter("wait-test-meter");

    // Counter with multiple labels
    let request_counter = meter.u64_counter("http_requests_total").build();
    request_counter.add(
        100,
        &[
            opentelemetry::KeyValue::new("method", "GET"),
            opentelemetry::KeyValue::new("status", "200"),
        ],
    );
    request_counter.add(
        50,
        &[
            opentelemetry::KeyValue::new("method", "POST"),
            opentelemetry::KeyValue::new("status", "201"),
        ],
    );
    request_counter.add(
        10,
        &[
            opentelemetry::KeyValue::new("method", "GET"),
            opentelemetry::KeyValue::new("status", "404"),
        ],
    );
    println!("   ✓ HTTP request counters recorded");

    // Gauge for system metrics
    let memory_gauge = meter.f64_gauge("memory_usage_bytes").build();
    memory_gauge.record(1024.0 * 1024.0 * 512.0, &[opentelemetry::KeyValue::new("type", "heap")]);
    println!("   ✓ Memory gauge recorded");

    // Histogram for latencies
    let latency_histogram = meter.f64_histogram("request_duration_seconds").build();
    for &duration in &[0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0] {
        latency_histogram
            .record(duration, &[opentelemetry::KeyValue::new("endpoint", "/api/test")]);
    }
    println!("   ✓ Latency histogram recorded\n");

    println!("⏳ Waiting 35 seconds for metrics export...");
    println!("   (Console exporter exports every 30 seconds)\n");
    println!("📋 Expected output format:");
    println!("   - Resource: service.name, service.namespace, etc.");
    println!("   - Scope: meter name, version, etc.");
    println!("   - Metrics: counter, gauge, histogram with their values\n");

    // Wait for export
    thread::sleep(Duration::from_secs(35));

    println!("\n✅ Export should have appeared above\n");

    // Shutdown
    exporter.shutdown().expect("Failed to shutdown");
    println!("✅ Test completed\n");
}

#[test]
fn test_multiple_exporters_comparison() {
    println!("\n🧪 Testing Different Exporter Types\n");

    // Test Prometheus exporter
    println!("📊 Testing Prometheus Exporter:");
    let prom_exporter = MetricsExporter::new(ExporterType::Prometheus);
    assert!(prom_exporter.init("prom-service").is_ok());
    assert!(prom_exporter.get_prometheus_registry().is_some());
    println!("   ✅ Prometheus exporter works, has registry\n");
    prom_exporter.shutdown().ok();

    // Test Console exporter
    println!("🖥️  Testing Console Exporter:");
    let console_exporter = MetricsExporter::new(ExporterType::Console);
    assert!(console_exporter.init("console-service").is_ok());
    assert!(console_exporter.get_prometheus_registry().is_none());
    println!("   ✅ Console exporter works, no registry (expected)\n");
    console_exporter.shutdown().ok();

    println!("✅ Exporter comparison test completed\n");
}
