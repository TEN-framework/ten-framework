//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "ten_runtime/binding/cpp/ten.h"

namespace ten {

class default_app_t : public ten::app_t {};

}  // namespace ten

int main() {
  auto *app = new ten::default_app_t();
  app->run();
  delete app;
  return 0;
}
