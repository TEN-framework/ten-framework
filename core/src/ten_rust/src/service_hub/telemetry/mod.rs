//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod counter;
pub use counter::*;

pub mod histogram;
pub use histogram::*;

pub mod gauge;
use std::{ffi::CStr, os::raw::c_char, ptr};

use actix_web::{web, HttpResponse};
pub use gauge::*;
use prometheus::{Encoder, Registry, TextEncoder};

use super::ServiceHub;
use crate::constants::METRICS;

pub enum MetricHandle {
    Counter(prometheus::Counter),
    CounterVec(prometheus::CounterVec),
    Gauge(prometheus::Gauge),
    GaugeVec(prometheus::GaugeVec),
    Histogram(prometheus::Histogram),
    HistogramVec(prometheus::HistogramVec),
}

pub fn configure_telemetry_route(cfg: &mut web::ServiceConfig, registry: Registry) {
    cfg.route(
        METRICS,
        web::get().to(move || {
            let registry = registry.clone();
            async move {
                let metric_families = registry.gather();
                let encoder = TextEncoder::new();
                let mut buffer = Vec::new();

                if encoder.encode(&metric_families, &mut buffer).is_err() {
                    return HttpResponse::InternalServerError().finish();
                }

                match String::from_utf8(buffer) {
                    Ok(response) => HttpResponse::Ok().body(response),
                    Err(_) => HttpResponse::InternalServerError().finish(),
                }
            }
        }),
    );
}

unsafe fn convert_label_names(
    names_ptr: *const *const c_char,
    names_len: usize,
) -> Option<Vec<String>> {
    if names_ptr.is_null() {
        return Some(vec![]);
    }

    let mut result = Vec::with_capacity(names_len);

    for i in 0..names_len {
        let c_str_ptr = *names_ptr.add(i);
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

/// Create a metric.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_create(
    system_ptr: *mut ServiceHub,
    metric_type: u32, // 0=Counter, 1=Gauge, 2=Histogram
    name: *const c_char,
    help: *const c_char,
    label_names_ptr: *const *const c_char,
    label_names_len: usize,
) -> *mut MetricHandle {
    debug_assert!(!system_ptr.is_null(), "System pointer is null");
    debug_assert!(!name.is_null(), "Name is null for metric creation");
    debug_assert!(!help.is_null(), "Help is null for metric creation");

    if system_ptr.is_null() || name.is_null() || help.is_null() {
        return ptr::null_mut();
    }

    let system = &mut *system_ptr;

    let name_str = match CStr::from_ptr(name).to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let help_str = match CStr::from_ptr(help).to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    let label_names_owned = match convert_label_names(label_names_ptr, label_names_len) {
        Some(v) => v,
        None => return ptr::null_mut(),
    };
    let label_names: Vec<&str> = label_names_owned.iter().map(|s| s.as_str()).collect();

    let metric_handle = match metric_type {
        0 => {
            // Counter.
            if label_names.is_empty() {
                match create_metric_counter(system, name_str, help_str) {
                    Ok(metric) => metric,
                    Err(_) => return ptr::null_mut(),
                }
            } else {
                match create_metric_counter_with_labels(system, name_str, help_str, &label_names) {
                    Ok(metric) => metric,
                    Err(_) => return ptr::null_mut(),
                }
            }
        }
        1 => {
            // Gauge.
            if label_names.is_empty() {
                match create_metric_gauge(system, name_str, help_str) {
                    Ok(metric) => metric,
                    Err(_) => return ptr::null_mut(),
                }
            } else {
                match create_metric_gauge_with_labels(system, name_str, help_str, &label_names) {
                    Ok(metric) => metric,
                    Err(_) => return ptr::null_mut(),
                }
            }
        }
        2 => {
            // Histogram.
            if label_names.is_empty() {
                match create_metric_histogram(system, name_str, help_str) {
                    Ok(metric) => metric,
                    Err(_) => return ptr::null_mut(),
                }
            } else {
                match create_metric_histogram_with_labels(system, name_str, help_str, &label_names)
                {
                    Ok(metric) => metric,
                    Err(_) => return ptr::null_mut(),
                }
            }
        }
        _ => return ptr::null_mut(),
    };

    Box::into_raw(Box::new(metric_handle))
}

/// Release a metric handle created by `ten_metric_create`.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub unsafe extern "C" fn ten_metric_destroy(metric_ptr: *mut MetricHandle) {
    debug_assert!(!metric_ptr.is_null(), "Metric pointer is null");

    if metric_ptr.is_null() {
        return;
    }

    drop(Box::from_raw(metric_ptr));
}
