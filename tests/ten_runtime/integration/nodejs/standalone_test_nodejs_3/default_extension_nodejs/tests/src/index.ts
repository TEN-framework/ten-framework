//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { AddonManager, App, TenEnv, TenErrorCode } from "ten-runtime-nodejs";
import { GreetingTester } from "./greeting.js";
import {
  AudioFrameTester,
  CmdTester,
  DataTester,
  TimeoutTester,
  VideoFrameTester,
} from "./basic_msg.js";
import assert from "assert";

// Note: The reason for this test case is that in mocha tests, if asan is
// enabled, it will catch leaks from mocha/npm itself, not from the TEN
// framework itself. So to ensure that the TEN framework itself has no leaks,
// we have this test case that doesn't use mocha, directly using the TEN
// framework to run the test case. This way, there's no custom
// lsan.suppressions file in this test case, and any leaks reported by lsan are
// definitely from the TEN framework.

let fakeApp: FakeApp;
let fakeAppRunPromise: Promise<void>;
const test_addon_name = "default_extension_nodejs";

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
              }
            ],
          },
        },
      })
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

async function main() {
  // TODO(Wei): It needs to be changed to load addons/extensions on-demand
  // instead of loading everything at once. To achieve this, the Node.js addon
  // loader must also be usable when the app itself is a Node.js app.
  await AddonManager.getInstance().loadAllAddons();

  fakeApp = new FakeApp();
  fakeAppRunPromise = fakeApp.run();

  // wait for the app to be initialized
  await fakeApp.waitForInit();
  // END OF SETUP

  // TEST BODY

  const greetingMsg = "Hello, world!";
  const greetingTester = new GreetingTester(greetingMsg);
  greetingTester.setTestModeSingle(
    test_addon_name,
    `{"greetingMsg": "${greetingMsg}"}`,
  );
  const result = await greetingTester.run();
  assert(result === undefined, "result should be undefined");

  const greetingFailedTest = new GreetingTester(greetingMsg);
  greetingFailedTest.setTestModeSingle(
    test_addon_name,
    `{"greetingMsg": "xxx"}`,
  );
  const result2 = await greetingFailedTest.run();
  assert(result2 !== undefined, "result2 should not be undefined");
  assert(
    result2.errorCode === TenErrorCode.ErrorCodeGeneric,
    "result2 should be TenErrorCode.ErrorCodeGeneric",
  );
  assert(
    result2.errorMessage ===
    `Expected greeting message: ${greetingMsg}, but got: xxx`,
  );

  const cmdTester = new CmdTester();
  cmdTester.setTestModeSingle(test_addon_name, "{}");
  const result3 = await cmdTester.run();
  assert(result3 === undefined, "result3 should be undefined");

  const dataTester = new DataTester();
  dataTester.setTestModeSingle(test_addon_name, "{}");
  const result4 = await dataTester.run();
  assert(result4 === undefined, "result4 should be undefined");

  const videoFrameTester = new VideoFrameTester();
  videoFrameTester.setTestModeSingle(test_addon_name, "{}");
  const result5 = await videoFrameTester.run();
  assert(result5 === undefined, "result5 should be undefined");

  const audioFrameTester = new AudioFrameTester();
  audioFrameTester.setTestModeSingle(test_addon_name, "{}");
  const result6 = await audioFrameTester.run();
  assert(result6 === undefined, "result6 should be undefined");

  const timeoutTester = new TimeoutTester();
  timeoutTester.setTestModeSingle(test_addon_name, "{}");
  timeoutTester.setTimeout(1000 * 1000); // 1 second
  const result7 = await timeoutTester.run();
  assert(result7 !== undefined, "result7 should not be undefined");
  assert(
    result7.errorCode === TenErrorCode.ErrorCodeTimeout,
    "result7 should be TenErrorCode.ErrorCodeTimeout",
  );

  console.log("All tests passed");

  // END OF TEST BODY

  // TEARDOWN
  fakeApp.close();
  await fakeAppRunPromise;

  (global as unknown as { gc: () => void }).gc();
  // END OF TEARDOWN
}

main();
