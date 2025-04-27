//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <include_internal/ten_rust/ten_config.h>
#include <include_internal/ten_utils/schema/bindings/rust/schema_proxy.h>
#include <include_internal/ten_utils/value/bindings/rust/value_proxy.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct Cipher Cipher;
typedef struct MetricHandle MetricHandle;
typedef struct ServiceHub ServiceHub;
typedef struct ten_app_t ten_app_t;

TEN_RUST_PRIVATE_API void ten_rust_free_cstring(const char *ptr);

TEN_RUST_PRIVATE_API Cipher *ten_cipher_create(const char *algorithm,
                                               const char *params);

TEN_RUST_PRIVATE_API void ten_cipher_destroy(Cipher *cipher_ptr);

TEN_RUST_PRIVATE_API bool ten_cipher_encrypt_inplace(Cipher *cipher_ptr,
                                                     uint8_t *data,
                                                     uintptr_t data_len);

TEN_RUST_PRIVATE_API char *ten_remove_json_comments(
    const char *json_with_comments);

TEN_RUST_PRIVATE_API bool ten_validate_manifest_json_string(
    const char *manifest_data, const char **out_err_msg);

TEN_RUST_PRIVATE_API bool ten_validate_manifest_json_file(
    const char *manifest_file, const char **out_err_msg);

TEN_RUST_PRIVATE_API bool ten_validate_property_json_string(
    const char *property_data, const char **out_err_msg);

TEN_RUST_PRIVATE_API bool ten_validate_property_json_file(
    const char *property_file, const char **out_err_msg);

TEN_RUST_PRIVATE_API ServiceHub *ten_service_hub_create(
    const char *telemetry_host, uint32_t telemetry_port, const char *api_host,
    uint32_t api_port, ten_app_t *app);

TEN_RUST_PRIVATE_API void ten_service_hub_shutdown(ServiceHub *service_hub_ptr);

TEN_RUST_PRIVATE_API const char *ten_get_runtime_version(void);

TEN_RUST_PRIVATE_API MetricHandle *ten_metric_create(
    ServiceHub *system_ptr, uint32_t metric_type, const char *name,
    const char *help, const char *const *label_names_ptr,
    uintptr_t label_names_len);

TEN_RUST_PRIVATE_API void ten_metric_destroy(MetricHandle *metric_ptr);

TEN_RUST_PRIVATE_API void ten_metric_counter_inc(
    MetricHandle *metric_ptr, const char *const *label_values_ptr,
    uintptr_t label_values_len);

TEN_RUST_PRIVATE_API void ten_metric_counter_add(
    MetricHandle *metric_ptr, double value, const char *const *label_values_ptr,
    uintptr_t label_values_len);

TEN_RUST_PRIVATE_API void ten_metric_histogram_observe(
    MetricHandle *metric_ptr, double value, const char *const *label_values_ptr,
    uintptr_t label_values_len);

TEN_RUST_PRIVATE_API void ten_metric_gauge_set(
    MetricHandle *metric_ptr, double value, const char *const *label_values_ptr,
    uintptr_t label_values_len);

TEN_RUST_PRIVATE_API void ten_metric_gauge_inc(
    MetricHandle *metric_ptr, const char *const *label_values_ptr,
    uintptr_t label_values_len);

TEN_RUST_PRIVATE_API void ten_metric_gauge_dec(
    MetricHandle *metric_ptr, const char *const *label_values_ptr,
    uintptr_t label_values_len);

TEN_RUST_PRIVATE_API void ten_metric_gauge_add(
    MetricHandle *metric_ptr, double value, const char *const *label_values_ptr,
    uintptr_t label_values_len);

TEN_RUST_PRIVATE_API void ten_metric_gauge_sub(
    MetricHandle *metric_ptr, double value, const char *const *label_values_ptr,
    uintptr_t label_values_len);
