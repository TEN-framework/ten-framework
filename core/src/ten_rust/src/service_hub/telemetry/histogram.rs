//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::os::raw::c_char;

use anyhow::Result;

use super::{MetricHandle, ServiceHub};
use crate::service_hub::telemetry::convert_label_values;

pub fn create_metric_histogram(
    system: &mut ServiceHub,
    name_str: &str,
    help_str: &str,
) -> Result<MetricHandle> {
    let hist_opts = prometheus::HistogramOpts::new(name_str, help_str);
    match prometheus::Histogram::with_opts(hist_opts) {
        Ok(histogram) => {
            if let Err(e) = system.registry.register(Box::new(histogram.clone())) {
                eprintln!("Error registering histogram: {e:?}");
                return Err(anyhow::anyhow!("Error registering histogram"));
            }
            Ok(MetricHandle::Histogram(histogram))
        }
        Err(_) => Err(anyhow::anyhow!("Error creating histogram")),
    }
}

pub fn create_metric_histogram_with_labels(
    system: &mut ServiceHub,
    name_str: &str,
    help_str: &str,
    label_names: &[&str],
) -> Result<MetricHandle> {
    let hist_opts = prometheus::HistogramOpts::new(name_str, help_str);
    match prometheus::HistogramVec::new(hist_opts, label_names) {
        Ok(histogram_vec) => {
            if let Err(e) = system.registry.register(Box::new(histogram_vec.clone())) {
                eprintln!("Error registering histogram vec: {e:?}");
                return Err(anyhow::anyhow!("Error registering histogram"));
            }
            Ok(MetricHandle::HistogramVec(histogram_vec))
        }
        Err(_) => Err(anyhow::anyhow!("Error creating histogram")),
    }
}

/// Helper function to apply an operation to a histogram metric.
unsafe fn apply_to_histogram<F>(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
    op: F,
) where
    F: Fn(&prometheus::Histogram),
{
    debug_assert!(!metric_ptr.is_null(), "Metric pointer is null");

    if metric_ptr.is_null() {
        return;
    }

    let metric = &mut *metric_ptr;
    match metric {
        MetricHandle::Histogram(ref histogram) => {
            op(histogram);
        }
        MetricHandle::HistogramVec(ref histogram_vec) => {
            let values_owned = match convert_label_values(label_values_ptr, label_values_len) {
                Some(v) => v,
                None => return,
            };
            let label_refs: Vec<&str> = values_owned.iter().map(|s| s.as_str()).collect();
            if let Ok(histogram) = histogram_vec.get_metric_with_label_values(&label_refs) {
                op(&histogram);
            }
        }
        _ => {}
    }
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_histogram_observe(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_histogram(metric_ptr, label_values_ptr, label_values_len, |histogram| {
        histogram.observe(value)
    });
}
