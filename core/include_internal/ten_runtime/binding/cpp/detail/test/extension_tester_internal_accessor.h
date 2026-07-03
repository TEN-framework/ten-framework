//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/binding/cpp/detail/test/extension_tester.h"

namespace ten {

class extension_tester_internal_accessor_t {
 public:
  static void init_test_app_property_from_json(extension_tester_t &tester,
                                               const char *property_json_str) {
    TEN_ASSERT(property_json_str, "Invalid argument.");
    ten_extension_tester_init_test_app_property_from_json(
        tester.c_extension_tester, property_json_str);
  }
};

}  // namespace ten
