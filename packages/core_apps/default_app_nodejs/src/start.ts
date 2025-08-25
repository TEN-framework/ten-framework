//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import { App, TenEnv } from "ten-runtime-nodejs";

class DefaultApp extends App {
  async onConfigure(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("Default App onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("Default App onInit");
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("Default App onDeinit");
  }
}

const app = new DefaultApp();
app.run().then(() => {
  console.log("Default App run completed.");
});
