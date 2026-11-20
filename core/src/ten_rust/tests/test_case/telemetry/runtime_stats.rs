//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::{collections::BTreeMap, ffi::CString, ptr};

use opentelemetry::{global, KeyValue};
use serde_json::{json, Value};
use ten_rust::service_hub::telemetry::{
    ten_metric_create, ten_metric_destroy, ten_metric_gauge_set, ExporterType, MetricsExporter,
};

fn histogram(metrics: &Value) -> &Value {
    metrics["resourceMetrics"][0]["scopeMetrics"]
        .as_array()
        .unwrap()
        .iter()
        .flat_map(|scope| scope["metrics"].as_array().unwrap())
        .find(|metric| metric["name"] == "runtime_stats_test_duration")
        .unwrap()
}

fn fixed_values(metrics: &Value) -> BTreeMap<String, f64> {
    let metric = metrics["resourceMetrics"][0]["scopeMetrics"]
        .as_array()
        .unwrap()
        .iter()
        .flat_map(|scope| scope["metrics"].as_array().unwrap())
        .find(|metric| metric["name"] == "runtime_stats_test_fixed_duration")
        .unwrap();
    assert!(metric["gauge"].is_object(), "set values must be exported as a Gauge: {metric}");
    metric["gauge"]["dataPoints"]
        .as_array()
        .unwrap()
        .iter()
        .map(|point| {
            let graph = point["attributes"]
                .as_array()
                .unwrap()
                .iter()
                .find(|attribute| attribute["key"] == "graph_id")
                .unwrap()["value"]["stringValue"]
                .as_str()
                .unwrap();
            (graph.to_string(), point["asDouble"].as_f64().unwrap())
        })
        .collect()
}

#[test]
#[serial_test::serial]
fn runtime_stats_gauge_set_overwrites_and_retains_values_per_graph() {
    let exporter = MetricsExporter::new(ExporterType::Prometheus {
        service_name: Some("runtime-stats-gauge-test".into()),
    });
    exporter.init("unused").unwrap();
    let name = CString::new("runtime_stats_test_fixed_duration").unwrap();
    let help = CString::new("Fixed duration per graph").unwrap();
    let label_name = CString::new("graph_id").unwrap();
    let label_names = [label_name.as_ptr()];
    let first_graph = CString::new("graph-a").unwrap();
    let second_graph = CString::new("graph-b").unwrap();
    let first_labels = [first_graph.as_ptr()];
    let second_labels = [second_graph.as_ptr()];
    unsafe {
        let handle = ten_metric_create(
            ptr::null_mut(),
            1,
            name.as_ptr(),
            help.as_ptr(),
            label_names.as_ptr(),
            1,
        );
        assert!(!handle.is_null());
        ten_metric_gauge_set(handle, 100.0, first_labels.as_ptr(), 1);
        ten_metric_gauge_set(handle, 250.0, first_labels.as_ptr(), 1);
        ten_metric_gauge_set(handle, 400.0, second_labels.as_ptr(), 1);
        ten_metric_destroy(handle);
    }
    let expected = BTreeMap::from([("graph-a".to_string(), 250.0), ("graph-b".to_string(), 400.0)]);
    assert_eq!(fixed_values(&exporter.collect_metrics().unwrap()), expected);
    assert_eq!(fixed_values(&exporter.collect_metrics().unwrap()), expected);

    let families = exporter.get_prometheus_registry().unwrap().gather();
    let family = families
        .iter()
        .find(|family| family.name().starts_with("runtime_stats_test_fixed_duration"))
        .unwrap();
    let scraped: BTreeMap<String, f64> = family
        .get_metric()
        .iter()
        .map(|metric| {
            let graph = metric.get_label().iter().find(|label| label.name() == "graph_id").unwrap();
            (graph.value().to_string(), metric.get_gauge().as_ref().unwrap().value())
        })
        .collect();
    assert_eq!(scraped, expected);
    assert_eq!(fixed_values(&exporter.collect_metrics().unwrap()), expected);
    exporter.shutdown().unwrap();
}

#[test]
#[serial_test::serial]
fn runtime_stats_collects_cumulative_metrics_without_consuming_prometheus() {
    let exporter = MetricsExporter::new(ExporterType::Prometheus {
        service_name: Some("runtime-stats-test".into()),
    });
    exporter.init("unused").unwrap();
    let instrument = global::meter("runtime-stats-test")
        .f64_histogram("runtime_stats_test_duration")
        .with_description("Test duration")
        .with_unit("us")
        .with_boundaries(vec![10.0, 20.0])
        .build();
    let attributes = [KeyValue::new("extension_name", "worker")];
    for value in [5.0, 15.0, 25.0] {
        instrument.record(value, &attributes);
    }

    let first = exporter.collect_metrics().unwrap();
    let metric = histogram(&first);
    assert_eq!(metric["unit"], "us");
    assert_eq!(metric["description"], "Test duration");
    assert_eq!(metric["histogram"]["aggregationTemporality"], 2);
    let point = &metric["histogram"]["dataPoints"][0];
    assert_eq!(point["count"], 3);
    assert_eq!(point["sum"], 45.0);
    assert_eq!(point["explicitBounds"], json!([10.0, 20.0]));
    assert_eq!(point["bucketCounts"], json!([1, 1, 1]));
    assert_eq!(point["attributes"][0]["key"], "extension_name");
    assert_eq!(point["attributes"][0]["value"]["stringValue"], "worker");
    assert!(point["startTimeUnixNano"].as_str().is_some());
    assert!(point["timeUnixNano"].as_str().is_some());

    instrument.record(5.0, &attributes);
    let second = exporter.collect_metrics().unwrap();
    let point = &histogram(&second)["histogram"]["dataPoints"][0];
    assert_eq!(point["count"], 4);
    assert_eq!(point["sum"], 50.0);
    assert_eq!(point["bucketCounts"], json!([2, 1, 1]));

    let registry = exporter.get_prometheus_registry().unwrap();
    let families = registry.gather();
    let family = families
        .iter()
        .find(|family| family.name().starts_with("runtime_stats_test_duration"))
        .unwrap();
    let prometheus_histogram = family.get_metric()[0].get_histogram();
    assert_eq!(prometheus_histogram.get_sample_count(), 4);
    assert_eq!(prometheus_histogram.get_sample_sum(), 50.0);

    exporter.shutdown().unwrap();
    assert!(exporter.collect_metrics().is_err());
}
