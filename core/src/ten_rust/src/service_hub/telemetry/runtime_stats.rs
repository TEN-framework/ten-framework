//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use std::{
    ffi::{c_char, CString},
    ptr,
    sync::{Arc, Weak},
    time::Duration,
};

use anyhow::Result;
use opentelemetry_proto::tonic::collector::metrics::v1::ExportMetricsServiceRequest;
use opentelemetry_sdk::{
    error::OTelSdkResult,
    metrics::{
        data::ResourceMetrics, reader::MetricReader, InstrumentKind, ManualReader, Pipeline,
        Temporality,
    },
};
use serde_json::Value;

use crate::service_hub::ServiceHub;

/// A reader shared by the meter provider and the runtime property accessor.
/// Cumulative collection lets multiple callers read without consuming metrics.
#[derive(Clone, Debug, Default)]
pub(super) struct RuntimeMetricsReader(Arc<ManualReader>);

impl RuntimeMetricsReader {
    pub fn collect_json(&self) -> Result<Value> {
        let mut metrics = ResourceMetrics::default();
        self.collect(&mut metrics)?;
        Ok(serde_json::to_value(ExportMetricsServiceRequest::from(&metrics))?)
    }
}

impl MetricReader for RuntimeMetricsReader {
    fn register_pipeline(&self, pipeline: Weak<Pipeline>) {
        self.0.register_pipeline(pipeline);
    }

    fn collect(&self, metrics: &mut ResourceMetrics) -> OTelSdkResult {
        self.0.collect(metrics)
    }

    fn force_flush(&self) -> OTelSdkResult {
        self.0.force_flush()
    }

    fn shutdown_with_timeout(&self, timeout: Duration) -> OTelSdkResult {
        self.0.shutdown_with_timeout(timeout)
    }

    fn temporality(&self, kind: InstrumentKind) -> Temporality {
        self.0.temporality(kind)
    }
}

/// Collect runtime statistics as owned JSON for the virtual property accessor.
///
/// # Safety
///
/// `service_hub_ptr` must point to a live ServiceHub. `err_msg`, if provided,
/// must be writable. The returned string and error must be freed with
/// `ten_rust_free_cstring`. Collection must not race ServiceHub destruction.
#[no_mangle]
pub unsafe extern "C" fn ten_service_hub_get_runtime_stats(
    service_hub_ptr: *const ServiceHub,
    err_msg: *mut *mut c_char,
) -> *const c_char {
    if !err_msg.is_null() {
        *err_msg = ptr::null_mut();
    }

    let result = (|| -> Result<CString> {
        let service_hub =
            service_hub_ptr.as_ref().ok_or_else(|| anyhow::anyhow!("Service hub is null"))?;
        let metrics = service_hub.metrics_exporter.collect_metrics()?;
        Ok(CString::new(serde_json::to_string(&serde_json::json!({ "metrics": metrics }))?)?)
    })();

    match result {
        Ok(json) => json.into_raw(),
        Err(error) => {
            if !err_msg.is_null() {
                *err_msg = CString::new(error.to_string()).unwrap_or_default().into_raw();
            }
            ptr::null()
        }
    }
}
