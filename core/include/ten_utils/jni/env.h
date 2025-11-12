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
 * @brief: Enable jni
 * @param jvm: Java VM object
 */
TEN_UTILS_API void ten_jni_enable(JavaVM *jvm);

/**
 * @brief: Attach to current thread and fetch jni env
 * @return: jni env object of current thread, nullptr if jni not enabled
 * @note: You can assume this function always return valid jni env
 *        if |ten_jni_enable| already called.
 *        Will automatically detach when thread destroying
 */
TEN_UTILS_API JNIEnv *ten_jni_attach_current_thread();
