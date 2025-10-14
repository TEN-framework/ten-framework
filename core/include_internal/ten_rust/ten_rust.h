//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <include_internal/ten_rust/ten_config.h>
#include <include_internal/ten_rust/ten_rust.h>
#include <include_internal/ten_utils/log/log.h>
#include <include_internal/ten_utils/schema/bindings/rust/schema_proxy.h>
#include <include_internal/ten_utils/value/bindings/rust/value_proxy.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct Cipher Cipher;
typedef struct MetricHandle MetricHandle;
typedef struct ServiceHub ServiceHub;
typedef struct ten_app_t ten_app_t;
typedef struct AdvancedLogConfig AdvancedLogConfig;

/**
 * This function is a wrapper for the backtrace::trace and backtrace::resolve
 * functions. It is used to dump the backtrace of the current function. It is
 * called by the C function ten_rust_backtrace_dump.
 */
TEN_RUST_PRIVATE_API int ten_rust_backtrace_dump(
    void *ctx,
    int (*on_dump)(void *ctx, uintptr_t pc, const char *filename, int lineno_c,
                   const char *function, void *data),
    void (*on_error)(void *ctx, const char *msg, int errnum, void *data),
    uintptr_t skip);

/**
 * @brief Frees a C string that was allocated by Rust.
 *
 * This function takes ownership of a raw pointer and frees it. The caller must
 * ensure that the pointer was originally allocated by Rust and that it is not
 * used after being freed. Passing a null pointer is safe, as the function will
 * simply return in that case.
 *
 * @param ptr Pointer to the C string to be freed. Can be NULL.
 */
TEN_RUST_PRIVATE_API void ten_rust_free_cstring(const char *ptr);

/**
 * @brief Parses a JSON string into a GraphInfo and returns it as a JSON string.
 *
 * This function takes a C string containing JSON, parses it into a GraphInfo
 * structure, validates and processes it, then serializes it back to JSON.
 *
 * @param json_str A null-terminated C string containing the JSON representation
 *                 of a graph. Must not be NULL.
 * @param current_base_dir A null-terminated C string containing the current
 *                        base directory. Can be NULL if the current base
 *                        directory is not known.
 * @param err_msg Pointer to a char* that will be set to an error message if
 *                the function fails. Can be NULL if error details are not
 *                needed. If set, the error message must be freed using
 *                ten_rust_free_cstring().
 *
 * @return On success: A pointer to a newly allocated C string containing the
 *         processed graph JSON. On failure: NULL pointer.
 *
 * @note The caller must ensure that:
 *       - json_str is a valid null-terminated C string
 *       - current_base_dir is a valid null-terminated C string, or NULL
 *       - The returned pointer (if not null) is freed using
 *         ten_rust_free_cstring()
 *       - If err_msg is not NULL, the error message (if set) must be freed
 *         using ten_rust_free_cstring()
 *       - The input string contains valid UTF-8 encoded JSON
 *
 * @note Memory Management: Both the returned string and error message (if set)
 *       are allocated by Rust and must be freed by calling
 * ten_rust_free_cstring() when no longer needed.
 *
 * @example
 * ```c
 * const char* input_json = "{\"nodes\": []}";
 * const char* current_base_dir = "/path/to/current/base/dir";
 * char* err_msg = NULL;
 * const char* result =
 *     ten_rust_predefined_graph_validate_complete_flatten(
 *         input_json, current_base_dir, &err_msg);
 * if (result != NULL) {
 *     printf("Processed graph: %s\n", result);
 *     ten_rust_free_cstring(result);
 * } else if (err_msg != NULL) {
 *     printf("Failed to process graph: %s\n", err_msg);
 *     ten_rust_free_cstring(err_msg);
 * } else {
 *     printf("Failed to process graph\n");
 * }
 * ```
 */
TEN_RUST_PRIVATE_API const char *
ten_rust_predefined_graph_validate_complete_flatten(
    const char *json_str, const char *current_base_dir, char **err_msg);

TEN_RUST_PRIVATE_API const char *ten_rust_graph_validate_complete_flatten(
    const char *json_str, const char *current_base_dir,
    const char *src_loc_json_str, char **err_msg);

/**
 * @brief Validates a manifest API and returns it as a JSON string.
 *
 * This function takes a C string containing JSON, parses it into a ManifestApi
 * structure, validates and flattens it, then serializes it back to JSON. If
 * flattening is not needed, it will still return a new copy of the input JSON.
 *
 * @param manifest_api_json_str A null-terminated C string containing the JSON
 *                             representation of a manifest API. Must not be
 *                             NULL.
 * @param current_base_dir A null-terminated C string containing the current
 *                         base directory. Must not be NULL.
 * @param err_msg Pointer to a char* that will be set to an error message if
 *                the function fails. Can be NULL if error details are not
 *                needed. If set, the error message must be freed using
 *                ten_rust_free_cstring().
 *
 * @return On success: A pointer to a newly allocated C string containing either
 *         the flattened manifest API JSON or a copy of the input JSON. The
 *         caller is responsible for freeing this string using
 *         ten_rust_free_cstring().
 *         On failure: NULL pointer.
 *
 * @note The caller must ensure that:
 *       - manifest_api_json_str is a valid null-terminated C string
 *       - current_base_dir is a valid null-terminated C string
 *       - If err_msg is not NULL, the error message (if set) must be freed
 *         using ten_rust_free_cstring()
 *       - The input string contains valid UTF-8 encoded JSON
 *
 * @note Memory Management: Both the returned string (if not NULL) and error
 *       message (if set) are allocated by Rust and must be freed by calling
 *       ten_rust_free_cstring() when no longer needed.
 */
TEN_RUST_PRIVATE_API const char *ten_rust_manifest_api_flatten(
    const char *manifest_api_json_str, const char *current_base_dir,
    const char *app_base_dir, char **err_msg);

TEN_RUST_PRIVATE_API bool ten_rust_validate_graph_json_string(
    const char *graph_json_str, char **err_msg);

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

TEN_RUST_PRIVATE_API AdvancedLogConfig *ten_rust_create_log_config_from_json(
    const char *log_config_json, char **err_msg);

TEN_RUST_PRIVATE_API bool ten_rust_configure_log(AdvancedLogConfig *config,
                                                 bool reloadable,
                                                 char **err_msg);

TEN_RUST_PRIVATE_API bool ten_rust_log_reopen_all(AdvancedLogConfig *config,
                                                  bool reloadable,
                                                  char **err_msg);

TEN_RUST_PRIVATE_API void ten_rust_log(
    AdvancedLogConfig *config, const char *category, size_t category_len,
    int64_t pid, int64_t tid, int level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *msg, size_t msg_len);

TEN_RUST_PRIVATE_API void ten_rust_log_config_destroy(
    AdvancedLogConfig *config);
