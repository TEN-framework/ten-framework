//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use ten_rust::service_hub::telemetry::config::{ExporterType, OtlpProtocol, TelemetryConfig};

#[test]
fn test_parse_prometheus_config() {
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "enabled": true,
            "exporter": {
                "type": "prometheus",
                "config": {
                    "endpoint": "0.0.0.0:49483",
                    "path": "/metrics"
                }
            }
        }
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    assert!(config.enabled);
    assert_eq!(config.get_exporter_type(), ExporterType::Prometheus);

    let prom_config = config.get_prometheus_config().unwrap();
    assert_eq!(prom_config.endpoint, "0.0.0.0:49483");
    assert_eq!(prom_config.path, "/metrics");
    assert_eq!(config.get_prometheus_endpoint().unwrap(), "0.0.0.0:49483");
}

#[test]
fn test_parse_otlp_config() {
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "enabled": true,
            "exporter": {
                "type": "otlp",
                "config": {
                    "endpoint": "http://localhost:4317",
                    "protocol": "grpc",
                    "headers": {
                        "x-api-key": "secret"
                    }
                }
            }
        }
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    assert_eq!(config.get_exporter_type(), ExporterType::Otlp);

    let otlp = config.get_otlp_config().unwrap();
    assert_eq!(otlp.endpoint, "http://localhost:4317");
    assert_eq!(otlp.protocol, OtlpProtocol::Grpc);
    assert_eq!(otlp.headers.get("x-api-key").unwrap(), "secret");
}

#[test]
fn test_parse_console_config() {
    // Test without config field (should work since config is optional)
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "exporter": {
                "type": "console"
            }
        }
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    assert_eq!(config.get_exporter_type(), ExporterType::Console);

    // Test with empty config object
    let json_with_config = serde_json::json!({
        "enabled": true,
        "metrics": {
            "exporter": {
                "type": "console",
                "config": {}
            }
        }
    });

    let config_with_config = TelemetryConfig::from_json(&json_with_config).unwrap();
    assert_eq!(config_with_config.get_exporter_type(), ExporterType::Console);
}

#[test]
fn test_default_values() {
    let json = serde_json::json!({
        "enabled": true
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    assert!(config.is_metrics_enabled());
    assert_eq!(config.get_exporter_type(), ExporterType::Prometheus);
}

#[test]
fn test_disabled_telemetry() {
    let json = serde_json::json!({
        "enabled": false
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    assert!(!config.enabled);
}

#[test]
fn test_metrics_disabled() {
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "enabled": false
        }
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    assert!(config.enabled);
    assert!(!config.is_metrics_enabled());
}

#[test]
fn test_missing_prometheus_config() {
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "exporter": {
                "type": "prometheus",
                "config": {}
                // Empty config for prometheus
            }
        }
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    assert_eq!(config.get_exporter_type(), ExporterType::Prometheus);

    // When prometheus config is empty, it should be None (deserialized as Empty
    // variant) The default values are used in the exporter initialization
    assert!(config.get_prometheus_config().is_none());
}

#[test]
fn test_invalid_exporter_type() {
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "exporter": {
                "type": "invalid_type"
            }
        }
    });

    // Should fail to parse
    let result = TelemetryConfig::from_json(&json);
    assert!(result.is_err());
}

#[test]
fn test_otlp_http_protocol() {
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "exporter": {
                "type": "otlp",
                "config": {
                    "endpoint": "http://localhost:4318",
                    "protocol": "http"
                }
            }
        }
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    let otlp = config.get_otlp_config().unwrap();
    assert_eq!(otlp.protocol, OtlpProtocol::Http);
}

#[test]
fn test_otlp_without_headers() {
    let json = serde_json::json!({
        "enabled": true,
        "metrics": {
            "exporter": {
                "type": "otlp",
                "config": {
                    "endpoint": "http://localhost:4317",
                    "protocol": "grpc"
                }
            }
        }
    });

    let config = TelemetryConfig::from_json(&json).unwrap();
    let otlp = config.get_otlp_config().unwrap();
    assert!(otlp.headers.is_empty());
}
