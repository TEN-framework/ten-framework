//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { GreetingTester } from "./greeting.js";
import {
  AudioFrameTester,
  CmdTester,
  DataTester,
  TimeoutTester,
  VideoFrameTester,
} from "./basic_msg.js";
import assert from "assert";
import { TenErrorCode } from "ten-runtime-nodejs";

const test_addon_name = "default_extension_nodejs";

describe("MyExtensionTester", () => {
  it("greeting", async () => {
    const greetingMsg = "Hello, world!";
    const extensionTester = new GreetingTester(greetingMsg);
    extensionTester.setTestModeSingle(
      test_addon_name,
      `{"greetingMsg": "${greetingMsg}"}`,
    );
    await extensionTester.run();
  });

  it("greeting_failed", async () => {
    const greetingMsg = "Hello, world!";
    const extensionTester = new GreetingTester(greetingMsg);
    extensionTester.setTestModeSingle(
      test_addon_name,
      `{"greetingMsg": "xxx"}`,
    );
    const result = await extensionTester.run();
    assert(result !== undefined, "result should not be undefined");
    assert(
      result.errorCode === TenErrorCode.ErrorCodeGeneric,
      "result should be TenErrorCode.ErrorCodeGeneric",
    );
    assert(
      result.errorMessage ===
        `Expected greeting message: ${greetingMsg}, but got: xxx`,
    );
  });

  it("cmd", async () => {
    const extensionTester = new CmdTester();
    extensionTester.setTestModeSingle(test_addon_name, "{}");
    await extensionTester.run();
  });

  it("data", async () => {
    const extensionTester = new DataTester();
    extensionTester.setTestModeSingle(test_addon_name, "{}");
    await extensionTester.run();
  });

  it("videoFrame", async () => {
    const extensionTester = new VideoFrameTester();
    extensionTester.setTestModeSingle(test_addon_name, "{}");
    await extensionTester.run();
  });

  it("audioFrame", async () => {
    const extensionTester = new AudioFrameTester();
    extensionTester.setTestModeSingle(test_addon_name, "{}");
    await extensionTester.run();
  });

  it("timeout", async () => {
    const extensionTester = new TimeoutTester();
    extensionTester.setTestModeSingle(test_addon_name, "{}");
    extensionTester.setTimeout(1000 * 1000); // 1 second
    const result = await extensionTester.run();
    assert(result !== undefined, "result should not be undefined");
    assert(
      result.errorCode === TenErrorCode.ErrorCodeTimeout,
      "result should be TenErrorCode.ErrorCodeTimeout",
    );
  });
});
