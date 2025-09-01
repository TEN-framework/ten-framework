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

pub fn create_metric_gauge(
    system: &mut ServiceHub,
    name_str: &str,
    help_str: &str,
) -> Result<MetricHandle> {
    let gauge_opts = prometheus::Opts::new(name_str, help_str);
    match prometheus::Gauge::with_opts(gauge_opts) {
        Ok(gauge) => {
            if let Err(e) = system.registry.register(Box::new(gauge.clone())) {
                eprintln!("Error registering gauge: {e:?}");
                return Err(anyhow::anyhow!("Error registering gauge"));
            }
            Ok(MetricHandle::Gauge(gauge))
        }
        Err(_) => Err(anyhow::anyhow!("Error creating gauge")),
    }
}

pub fn create_metric_gauge_with_labels(
    system: &mut ServiceHub,
    name_str: &str,
    help_str: &str,
    label_names: &[&str],
) -> Result<MetricHandle> {
    let gauge_opts = prometheus::Opts::new(name_str, help_str);
    match prometheus::GaugeVec::new(gauge_opts, label_names) {
        Ok(gauge_vec) => {
            if let Err(e) = system.registry.register(Box::new(gauge_vec.clone())) {
                eprintln!("Error registering gauge vec: {e:?}");
                return Err(anyhow::anyhow!("Error registering gauge"));
            }
            Ok(MetricHandle::GaugeVec(gauge_vec))
        }
        Err(_) => Err(anyhow::anyhow!("Error creating gauge")),
    }
}

/// Helper function to apply an operation to a gauge metric.
unsafe fn apply_to_gauge<F>(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
    op: F,
) where
    F: Fn(&prometheus::Gauge),
{
    debug_assert!(!metric_ptr.is_null(), "Metric pointer is null");

    if metric_ptr.is_null() {
        return;
    }

    let metric = &mut *metric_ptr;
    match metric {
        MetricHandle::Gauge(ref gauge) => {
            op(gauge);
        }
        MetricHandle::GaugeVec(ref gauge_vec) => {
            let values_owned = match convert_label_values(label_values_ptr, label_values_len) {
                Some(v) => v,
                None => return,
            };
            let label_refs: Vec<&str> = values_owned.iter().map(|s| s.as_str()).collect();
            if let Ok(gauge) = gauge_vec.get_metric_with_label_values(&label_refs) {
                op(&gauge);
            }
        }
        _ => {}
    }
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_gauge_set(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_gauge(metric_ptr, label_values_ptr, label_values_len, |gauge| gauge.set(value));
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_gauge_inc(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_gauge(metric_ptr, label_values_ptr, label_values_len, |gauge| gauge.inc());
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_gauge_dec(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_gauge(metric_ptr, label_values_ptr, label_values_len, |gauge| gauge.dec());
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_gauge_add(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_gauge(metric_ptr, label_values_ptr, label_values_len, |gauge| gauge.add(value));
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_gauge_sub(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_gauge(metric_ptr, label_values_ptr, label_values_len, |gauge| gauge.sub(value));
}
