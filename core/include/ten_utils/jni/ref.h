//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <jni.h>

/**
 * @brief: Add a global strong reference to object
 * @param obj: The object
 * @return: The object handle
 */
TEN_UTILS_API jobject ten_jni_ref_new_global(jobject obj);

/**
 * @brief: Delete a global strong reference to object
 * @param obj: The object
 */
TEN_UTILS_API void ten_jni_ref_del_global(jobject obj);

/**
 * @brief: Add a global weak reference to object
 * @param obj: The object
 * @return: The object handle
 */
TEN_UTILS_API jobject ten_jni_ref_new_weak_global(jobject obj);

/**
 * @brief: Delete a global weak reference to object
 * @param obj: The object
 */
TEN_UTILS_API void ten_jni_ref_del_weak_global(jobject obj);

/**
 * @brief: Add a local reference to object
 * @param obj: The object
 * @return: The object handle
 */
TEN_UTILS_API jobject ten_jni_ref_new_local(jobject obj);

/**
 * @brief: Delete a local reference to object
 * @param obj: The object
 */
TEN_UTILS_API void ten_jni_ref_del_local(jobject obj);

/**
 * @brief: Check whether two object represent same Java object
 * @param left: One object
 * @param right: Another object
 */
TEN_UTILS_API int ten_jni_ref_is_same(jobject left, jobject right);
