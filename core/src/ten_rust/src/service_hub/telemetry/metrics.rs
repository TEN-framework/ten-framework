//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::{ffi::CStr, os::raw::c_char};

use opentelemetry::{global, KeyValue};

// ============================================================================
// High-level Metrics API (Backend-agnostic)
// ============================================================================

/// Record a counter metric
#[allow(dead_code)]
pub fn record_counter(name: &str, value: u64, labels: &[(&str, &str)]) {
    let meter = global::meter("ten-framework");
    let counter = meter.u64_counter(name.to_string()).build();

    let attributes: Vec<KeyValue> =
        labels.iter().map(|(k, v)| KeyValue::new(k.to_string(), v.to_string())).collect();

    counter.add(value, &attributes);
}

/// Record a gauge metric (using up_down_counter in OpenTelemetry)
#[allow(dead_code)]
pub fn record_gauge(name: &str, value: f64, labels: &[(&str, &str)]) {
    let meter = global::meter("ten-framework");
    let gauge = meter.f64_up_down_counter(name.to_string()).build();

    let attributes: Vec<KeyValue> =
        labels.iter().map(|(k, v)| KeyValue::new(k.to_string(), v.to_string())).collect();

    gauge.add(value, &attributes);
}

/// Record a histogram metric
#[allow(dead_code)]
pub fn record_histogram(name: &str, value: f64, labels: &[(&str, &str)]) {
    let meter = global::meter("ten-framework");
    let histogram = meter.f64_histogram(name.to_string()).build();

    let attributes: Vec<KeyValue> =
        labels.iter().map(|(k, v)| KeyValue::new(k.to_string(), v.to_string())).collect();

    histogram.record(value, &attributes);
}

// ============================================================================
// C FFI Interface
// ============================================================================

#[repr(C)]
pub struct MetricHandle {
    name: String,
    metric_type: MetricType,
    label_names: Vec<String>,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum MetricType {
    Counter = 0,
    Gauge = 1,
    Histogram = 2,
}

/// Create a metric handle
///
/// # Safety
///
/// This function is unsafe because it dereferences raw pointers passed from C.
/// The caller must ensure that:
/// - `name` is a valid, non-null pointer to a null-terminated C string
/// - The C string remains valid for the duration of the call
#[no_mangle]
pub unsafe extern "C" fn ten_metric_create(
    _system_ptr: *mut std::ffi::c_void,
    metric_type: u32,
    name: *const c_char,
    _help: *const c_char,
    label_names_ptr: *const *const c_char,
    label_names_len: usize,
) -> *mut MetricHandle {
    if name.is_null() {
        return std::ptr::null_mut();
    }

    let name_str = match CStr::from_ptr(name).to_str() {
        Ok(s) => s.to_string(),
        Err(_) => return std::ptr::null_mut(),
    };

    let metric_type = match metric_type {
        0 => MetricType::Counter,
        1 => MetricType::Gauge,
        2 => MetricType::Histogram,
        _ => return std::ptr::null_mut(),
    };

    // Parse label names from C
    let mut label_names = Vec::new();
    if !label_names_ptr.is_null() && label_names_len > 0 {
        let label_names_slice = std::slice::from_raw_parts(label_names_ptr, label_names_len);

        for &label_name_ptr in label_names_slice {
            if !label_name_ptr.is_null() {
                if let Ok(name_str) = CStr::from_ptr(label_name_ptr).to_str() {
                    label_names.push(name_str.to_string());
                }
            }
        }
    }

    let handle = MetricHandle {
        name: name_str,
        metric_type,
        label_names,
    };

    Box::into_raw(Box::new(handle))
}

/// Destroy a metric handle
///
/// # Safety
///
/// This function is unsafe because it takes ownership of a raw pointer.
/// The caller must ensure that:
/// - `metric_ptr` is either null or was previously created by
///   `ten_metric_create`
/// - `metric_ptr` has not been freed previously
/// - After this call, `metric_ptr` must not be used again
#[no_mangle]
pub unsafe extern "C" fn ten_metric_destroy(metric_ptr: *mut MetricHandle) {
    if !metric_ptr.is_null() {
        drop(Box::from_raw(metric_ptr));
    }
}

// Helper to convert C string array to Rust Vec
#[allow(dead_code)]
unsafe fn convert_label_values(values_ptr: *const *const c_char, values_len: usize) -> Vec<String> {
    if values_ptr.is_null() || values_len == 0 {
        return Vec::new();
    }

    let mut result = Vec::with_capacity(values_len);
    for i in 0..values_len {
        let c_str_ptr = *values_ptr.add(i);
        if !c_str_ptr.is_null() {
            if let Ok(s) = CStr::from_ptr(c_str_ptr).to_str() {
                result.push(s.to_string());
            }
        }
    }
    result
}

// ============================================================================
// Counter operations
// ============================================================================

/// Increment counter by 1
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_counter_inc(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    ten_metric_counter_add(metric_ptr, 1.0, label_values_ptr, label_values_len);
}

/// Add value to counter
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_counter_add(
    metric_ptr: *mut MetricHandle,
    value: f64,
    _label_values_ptr: *const *const c_char,
    _label_values_len: usize,
) {
    if metric_ptr.is_null() {
        return;
    }

    let metric = &*metric_ptr;
    let meter = global::meter("ten-framework");
    let counter = meter.u64_counter(metric.name.clone()).build();

    // TODO: Support labels
    counter.add(value as u64, &[]);
}

// ============================================================================
// Gauge operations
// ============================================================================

/// Set gauge to a specific value
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_gauge_set(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    if metric_ptr.is_null() {
        return;
    }

    let metric = &*metric_ptr;
    let meter = global::meter("ten-framework");
    let gauge = meter.f64_up_down_counter(metric.name.clone()).build();

    // Parse label values from C and match with label names
    let mut attributes = Vec::new();
    if !label_values_ptr.is_null() && label_values_len > 0 {
        let label_values_slice = std::slice::from_raw_parts(label_values_ptr, label_values_len);

        // Match label names with label values
        for (i, &label_value_ptr) in label_values_slice.iter().enumerate() {
            if !label_value_ptr.is_null() && i < metric.label_names.len() {
                if let Ok(value_str) = CStr::from_ptr(label_value_ptr).to_str() {
                    attributes
                        .push(KeyValue::new(metric.label_names[i].clone(), value_str.to_string()));
                }
            }
        }
    }

    gauge.add(value, &attributes);
}

/// Increment gauge by 1
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_gauge_inc(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    ten_metric_gauge_add(metric_ptr, 1.0, label_values_ptr, label_values_len);
}

/// Decrement gauge by 1
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_gauge_dec(
    metric_ptr: *mut MetricHandle,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    ten_metric_gauge_sub(metric_ptr, 1.0, label_values_ptr, label_values_len);
}

/// Add value to gauge
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_gauge_add(
    metric_ptr: *mut MetricHandle,
    value: f64,
    _label_values_ptr: *const *const c_char,
    _label_values_len: usize,
) {
    if metric_ptr.is_null() {
        return;
    }

    let metric = &*metric_ptr;
    let meter = global::meter("ten-framework");
    let gauge = meter.f64_up_down_counter(metric.name.clone()).build();

    // TODO: Support labels
    gauge.add(value, &[]);
}

/// Subtract value from gauge
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_gauge_sub(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    ten_metric_gauge_add(metric_ptr, -value, label_values_ptr, label_values_len);
}

// ============================================================================
// Histogram operations
// ============================================================================

/// Record a histogram observation
///
/// # Safety
///
/// This function is unsafe because it dereferences a raw pointer.
/// The caller must ensure that `metric_ptr` is a valid pointer to a
/// MetricHandle.
#[no_mangle]
pub unsafe extern "C" fn ten_metric_histogram_observe(
    metric_ptr: *mut MetricHandle,
    value: f64,
    label_values_ptr: *const *const c_char,
    label_values_len: usize,
) {
    if metric_ptr.is_null() {
        return;
    }

    let metric = &*metric_ptr;
    let meter = global::meter("ten-framework");
    let histogram = meter.f64_histogram(metric.name.clone()).build();

    // Convert label values from C strings to Rust
    let mut attributes = Vec::new();
    if !label_values_ptr.is_null() && label_values_len > 0 {
        let label_values = std::slice::from_raw_parts(label_values_ptr, label_values_len);

        for (i, &label_value_ptr) in label_values.iter().enumerate() {
            if i < metric.label_names.len() && !label_value_ptr.is_null() {
                let label_name = &metric.label_names[i];
                let label_value = CStr::from_ptr(label_value_ptr).to_str().unwrap_or("");

                attributes.push(KeyValue::new(label_name.clone(), label_value.to_string()));
            }
        }
    }

    histogram.record(value, &attributes);
}
