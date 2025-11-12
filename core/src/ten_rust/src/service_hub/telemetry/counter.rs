//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{ffi::CStr, os::raw::c_char};

use anyhow::Result;

use super::{MetricHandle, ServiceHub};

unsafe fn convert_label_values(
    values_ptr: *const *const c_char,
    values_len: usize,
) -> Option<Vec<String>> {
    if values_ptr.is_null() {
        return Some(vec![]);
    }

    let mut result = Vec::with_capacity(values_len);

    for i in 0..values_len {
        let c_str_ptr = *values_ptr.add(i);
        if c_str_ptr.is_null() {
            return None;
        }

        let c_str = CStr::from_ptr(c_str_ptr);
        match c_str.to_str() {
            Ok(s) => result.push(s.to_string()),
            Err(_) => return None,
        }
    }

    Some(result)
}

pub fn create_metric_counter(
    system: &mut ServiceHub,
    name_str: &str,
    help_str: &str,
) -> Result<MetricHandle> {
    let counter_opts = prometheus::Opts::new(name_str, help_str);
    match prometheus::Counter::with_opts(counter_opts) {
        Ok(counter) => {
            if let Err(e) = system.registry.register(Box::new(counter.clone())) {
                eprintln!("Error registering counter: {e:?}");
                return Err(anyhow::anyhow!("Error registering counter"));
            }
            Ok(MetricHandle::Counter(counter))
        }
        Err(_) => Err(anyhow::anyhow!("Error creating counter")),
    }
}

pub fn create_metric_counter_with_labels(
    system: &mut ServiceHub,
    name_str: &str,
    help_str: &str,
    label_names: &[&str],
) -> Result<MetricHandle> {
    let counter_opts = prometheus::Opts::new(name_str, help_str);
    match prometheus::CounterVec::new(counter_opts, label_names) {
        Ok(counter_vec) => {
            if let Err(e) = system.registry.register(Box::new(counter_vec.clone())) {
                eprintln!("Error registering counter vec: {e:?}");
                return Err(anyhow::anyhow!("Error registering counter"));
            }
            Ok(MetricHandle::CounterVec(counter_vec))
        }
        Err(_) => Err(anyhow::anyhow!("Error creating counter")),
    }
}

/// Helper function to apply an operation to a counter metric.
unsafe fn apply_to_counter<F>(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
    op: F,
) where
    F: Fn(&prometheus::Counter),
{
    debug_assert!(!metric_ptr.is_null(), "Metric pointer is null");

    if metric_ptr.is_null() {
        return;
    }

    let metric = &mut *metric_ptr;
    match metric {
        MetricHandle::Counter(ref counter) => {
            op(counter);
        }
        MetricHandle::CounterVec(ref counter_vec) => {
            let values_owned = match convert_label_values(label_values_ptr, label_values_len) {
                Some(v) => v,
                None => return,
            };
            let label_refs: Vec<&str> = values_owned.iter().map(|s| s.as_str()).collect();
            if let Ok(counter) = counter_vec.get_metric_with_label_values(&label_refs) {
                op(&counter);
            }
        }
        _ => {}
    }
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_counter_inc(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_counter(metric_ptr, label_values_ptr, label_values_len, |counter| counter.inc());
}

#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_counter_add(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    apply_to_counter(metric_ptr, label_values_ptr, label_values_len, |counter| {
        counter.inc_by(value)
    });
}
