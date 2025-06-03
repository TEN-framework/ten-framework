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
  VideoFrameTester,
} from "./basic_msg.js";

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
});
