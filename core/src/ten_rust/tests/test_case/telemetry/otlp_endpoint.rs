//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::{
    env,
    ffi::OsString,
    io::{Read, Write},
    net::TcpListener,
    thread,
    time::{Duration, Instant},
};

use opentelemetry::global;
use serde_json::{json, Value};
use ten_rust::{
    json_schema::ten_validate_property_json_string,
    service_hub::telemetry::{
        config::TelemetryConfig,
        exporter::{ExporterType, MetricsExporter},
    },
};

const METRICS_ENDPOINT: &str = "OTEL_EXPORTER_OTLP_METRICS_ENDPOINT";
const COMMON_ENDPOINT: &str = "OTEL_EXPORTER_OTLP_ENDPOINT";
const COMPRESSION: &str = "OTEL_EXPORTER_OTLP_METRICS_COMPRESSION";

struct EndpointEnv(Vec<(&'static str, Option<OsString>)>);

impl EndpointEnv {
    fn set(metrics: Option<&str>, common: Option<&str>) -> Self {
        let previous = [METRICS_ENDPOINT, COMMON_ENDPOINT, COMPRESSION]
            .into_iter()
            .map(|name| (name, env::var_os(name)))
            .collect();
        for (name, value) in
            [(METRICS_ENDPOINT, metrics), (COMMON_ENDPOINT, common), (COMPRESSION, None)]
        {
            match value {
                Some(value) => env::set_var(name, value),
                None => env::remove_var(name),
            }
        }
        Self(previous)
    }
}

impl Drop for EndpointEnv {
    fn drop(&mut self) {
        for (name, value) in &self.0 {
            match value {
                Some(value) => env::set_var(name, value),
                None => env::remove_var(name),
            }
        }
    }
}

fn config(exporter: Value) -> TelemetryConfig {
    TelemetryConfig::from_json(&json!({
        "enabled": true,
        "metrics": { "enabled": true, "exporter": exporter }
    }))
    .unwrap()
}

fn assert_local_metrics_work(exporter: &MetricsExporter) {
    global::meter("otlp-fallback-test").f64_gauge("otlp_fallback_value").build().record(42.0, &[]);
    let stats = exporter.collect_metrics().unwrap();
    let value = stats["resourceMetrics"][0]["scopeMetrics"]
        .as_array()
        .unwrap()
        .iter()
        .flat_map(|scope| scope["metrics"].as_array().unwrap())
        .find(|metric| metric["name"] == "otlp_fallback_value")
        .unwrap();
    assert_eq!(value["gauge"]["dataPoints"][0]["asDouble"], 42.0);
}

#[test]
#[serial_test::serial]
fn otlp_endpoint_may_be_omitted_from_every_config_form() {
    // A common OTLP endpoint must not activate metrics export on its own.
    let _env = EndpointEnv::set(None, Some("http://127.0.0.1:4317/common"));
    for exporter_json in [
        json!({"type": "otlp"}),
        json!({"type": "otlp", "config": {}}),
        json!({"type": "otlp", "config": {"protocol": "grpc"}}),
        json!({"type": "otlp", "config": {"endpoint": ""}}),
        json!({"type": "otlp", "config": {"endpoint": "   "}}),
    ] {
        let cfg = config(exporter_json.clone());
        assert!(cfg.get_otlp_config().is_some(), "{exporter_json}");
        let selected = ExporterType::from_config(&cfg);
        assert!(!matches!(selected, ExporterType::Otlp { .. }), "{exporter_json}");
        let exporter = MetricsExporter::new(selected);
        exporter.init("otlp-fallback-test").unwrap();
        assert!(exporter.get_prometheus_registry().is_none());
        assert_local_metrics_work(&exporter);
        exporter.shutdown().unwrap();
    }
}

#[test]
fn otlp_property_schema_accepts_missing_and_empty_endpoint() {
    for exporter in [
        json!({"type": "otlp"}),
        json!({"type": "otlp", "config": {"protocol": "grpc"}}),
        json!({"type": "otlp", "config": {"endpoint": ""}}),
    ] {
        let property = json!({"ten": {"services": {"telemetry": {
            "enabled": true, "metrics": {"enabled": true, "exporter": exporter}
        }}}});
        ten_validate_property_json_string(&property.to_string()).unwrap();
    }
}

#[test]
#[serial_test::serial]
fn otlp_endpoint_uses_config_then_metrics_environment() {
    for (explicit, metrics, common, expected) in [
        (
            Some("http://127.0.0.1:4317/config"),
            Some("http://127.0.0.1:4317/signal"),
            Some("http://127.0.0.1:4317/common"),
            "http://127.0.0.1:4317/config",
        ),
        (
            None,
            Some("http://127.0.0.1:4317/signal"),
            Some("http://127.0.0.1:4317/common"),
            "http://127.0.0.1:4317/signal",
        ),
        (Some(""), Some("http://127.0.0.1:4317/signal"), None, "http://127.0.0.1:4317/signal"),
        (Some("   "), Some("http://127.0.0.1:4317/signal"), None, "http://127.0.0.1:4317/signal"),
    ] {
        let _env = EndpointEnv::set(metrics, common);
        let mut exporter = json!({"type": "otlp", "config": {"protocol": "grpc"}});
        if let Some(explicit) = explicit {
            exporter["config"]["endpoint"] = json!(explicit);
        }
        match ExporterType::from_config(&config(exporter)) {
            ExporterType::Otlp {
                endpoint, ..
            } => assert_eq!(endpoint, expected),
            other => panic!("OTLP should be enabled for a valid endpoint: {other:?}"),
        }
    }
}

#[test]
#[serial_test::serial]
fn otlp_invalid_endpoint_disables_export_without_losing_local_metrics() {
    for endpoint in [
        "",
        "   ",
        "not-a-url",
        "localhost:4317",
        "ftp://127.0.0.1:4317",
        "http://",
        "http://127.0.0.1:99999",
        "http://bad host:4317",
        "http://127.0.0.1:4317/#fragment",
    ] {
        let _env = EndpointEnv::set(Some(endpoint), Some("http://127.0.0.1:4317/valid"));
        let cfg = config(json!({"type": "otlp", "config": {"protocol": "grpc"}}));
        let selected = ExporterType::from_config(&cfg);
        assert!(!matches!(selected, ExporterType::Otlp { .. }), "{endpoint}");
        let exporter = MetricsExporter::new(selected);
        exporter.init("otlp-fallback-test").unwrap();
        assert!(exporter.get_prometheus_registry().is_none());
        assert_local_metrics_work(&exporter);
        exporter.shutdown().unwrap();
    }
}

#[test]
#[serial_test::serial]
fn otlp_builder_failure_falls_back_to_local_metrics() {
    let _env = EndpointEnv::set(None, None);
    env::set_var(COMPRESSION, "invalid-compression");
    let exporter = MetricsExporter::new(ExporterType::Otlp {
        endpoint: "http://127.0.0.1:4317".into(),
        protocol: ten_rust::service_hub::telemetry::OtlpProtocol::Grpc,
        headers: Default::default(),
        service_name: Some("otlp-fallback-test".into()),
    });
    exporter.init("unused").unwrap();
    assert_local_metrics_work(&exporter);
    exporter.shutdown().unwrap();
}

#[test]
#[serial_test::serial]
fn otlp_http_exports_to_the_resolved_endpoint() {
    for source in ["config", "metrics-env"] {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        listener.set_nonblocking(true).unwrap();
        let base = format!("http://{}", listener.local_addr().unwrap());
        let endpoint = format!("{base}/custom");
        let (metrics, common) = match source {
            "config" => (Some("not-a-url"), Some("not-a-url")),
            _ => (Some(endpoint.as_str()), Some("not-a-url")),
        };
        let _env = EndpointEnv::set(metrics, common);
        let collector = thread::spawn(move || {
            let deadline = Instant::now() + Duration::from_secs(5);
            let mut stream = loop {
                match listener.accept() {
                    Ok((stream, _)) => break stream,
                    Err(error) if error.kind() == std::io::ErrorKind::WouldBlock => {
                        assert!(Instant::now() < deadline, "OTLP HTTP export did not arrive");
                        thread::sleep(Duration::from_millis(10));
                    }
                    Err(error) => panic!("Collector failed: {error}"),
                }
            };
            stream.set_read_timeout(Some(Duration::from_secs(3))).unwrap();
            let mut request = Vec::new();
            let (headers, body_offset, content_length) = loop {
                let mut buffer = [0; 4096];
                let size = stream.read(&mut buffer).unwrap();
                assert_ne!(size, 0, "Incomplete OTLP HTTP request");
                request.extend_from_slice(&buffer[..size]);
                if let Some(offset) = request.windows(4).position(|bytes| bytes == b"\r\n\r\n") {
                    let headers = String::from_utf8(request[..offset].to_vec()).unwrap();
                    let content_length = headers
                        .to_ascii_lowercase()
                        .lines()
                        .find_map(|line| line.strip_prefix("content-length:"))
                        .unwrap()
                        .trim()
                        .parse::<usize>()
                        .unwrap();
                    break (headers, offset + 4, content_length);
                }
            };
            while request.len() < body_offset + content_length {
                let mut buffer = [0; 4096];
                let size = stream.read(&mut buffer).unwrap();
                assert_ne!(size, 0, "Incomplete OTLP HTTP body");
                request.extend_from_slice(&buffer[..size]);
            }
            stream
                .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
                .unwrap();
            assert!(headers.starts_with("POST /custom HTTP/1.1\r\n"), "{headers}");
            assert!(request[body_offset..]
                .windows(b"otlp_fallback_value".len())
                .any(|bytes| bytes == b"otlp_fallback_value"));
        });

        let mut exporter_json = json!({"type": "otlp", "config": {"protocol": "http"}});
        if source == "config" {
            exporter_json["config"]["endpoint"] = json!(endpoint);
        }
        let exporter = MetricsExporter::new(ExporterType::from_config(&config(exporter_json)));
        exporter.init("otlp-http-test").unwrap();
        assert_local_metrics_work(&exporter);
        exporter.shutdown().unwrap();
        collector.join().unwrap();
    }
}
