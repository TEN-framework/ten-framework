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
  private resolveInit: (() => void) | null = null;

  constructor() {
    super();
    this.initPromise = new Promise((resolve) => {
      this.resolveInit = resolve;
    });
  }

  async onInit(_tenEnv: TenEnv): Promise<void> {
    console.log("Default App onInit");
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
  assert(result === null, "result should be null");

  const greetingFailedTest = new GreetingTester(greetingMsg);
  greetingFailedTest.setTestModeSingle(
    test_addon_name,
    `{"greetingMsg": "xxx"}`,
  );
  const result2 = await greetingFailedTest.run();
  assert(result2 !== null, "result2 should not be null");
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
  assert(result3 === null, "result3 should be null");

  const dataTester = new DataTester();
  dataTester.setTestModeSingle(test_addon_name, "{}");
  const result4 = await dataTester.run();
  assert(result4 === null, "result4 should be null");

  const videoFrameTester = new VideoFrameTester();
  videoFrameTester.setTestModeSingle(test_addon_name, "{}");
  const result5 = await videoFrameTester.run();
  assert(result5 === null, "result5 should be null");

  const audioFrameTester = new AudioFrameTester();
  audioFrameTester.setTestModeSingle(test_addon_name, "{}");
  const result6 = await audioFrameTester.run();
  assert(result6 === null, "result6 should be null");

  const timeoutTester = new TimeoutTester();
  timeoutTester.setTestModeSingle(test_addon_name, "{}");
  timeoutTester.setTimeout(1000 * 1000); // 1 second
  const result7 = await timeoutTester.run();
  assert(result7 !== null, "result7 should not be null");
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
