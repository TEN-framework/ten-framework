//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { AddonManager, App, TenEnv } from "ten-runtime-nodejs";

let fakeApp: FakeApp;
let fakeAppRunPromise: Promise<void>;

class FakeApp extends App {
  private initPromise: Promise<void>;
  private resolveInit: (() => void) | undefined = undefined;

  constructor() {
    super();
    this.initPromise = new Promise((resolve) => {
      this.resolveInit = resolve;
    });
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    tenEnv.initPropertyFromJson(
      JSON.stringify({
        ten: {
          log: {
            handlers: [
              {
                matchers: [{ level: "debug" }],
                formatter: { type: "plain", colored: true },
                emitter: { type: "console", config: { stream: "stdout" } },
              },
            ],
          },
        },
      }),
    );
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("Default App onInit");
    if (this.resolveInit) {
      this.resolveInit();
    }
  }

  async waitForInit(): Promise<void> {
    return this.initPromise;
  }
}

before(async () => {
  // TODO(Wei): It needs to be changed to load addons/extensions on-demand
  // instead of loading everything at once. To achieve this, the Node.js addon
  // loader must also be usable when the app itself is a Node.js app.
  await AddonManager.getInstance().loadAllAddons();

  fakeApp = new FakeApp();
  fakeAppRunPromise = fakeApp.run();

  // Wait for the app to be initialized.
  await fakeApp.waitForInit();
});

after(async () => {
  fakeApp.close();
  await fakeAppRunPromise;

  (global as unknown as { gc: () => void }).gc();
});
