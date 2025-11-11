//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import assert from "assert";
import { MyExtensionTester } from "./index.js";

const test_addon_name = "vtt_nodejs";

describe("MyExtensionTester", () => {
  it("case1", async () => {
    const extensionTester = new MyExtensionTester();
    extensionTester.setTestModeSingle(test_addon_name, "{}");
    const result = await extensionTester.run();
    assert(result === undefined, "result should be undefined");

    console.log("deinit done");
  });

  it("case2", async () => {
    const extensionTester = new MyExtensionTester();
    extensionTester.setTestModeSingle(test_addon_name, "{}");
    const result = await extensionTester.run();
    assert(result === undefined, "result should be undefined");

    console.log("deinit done");
  });
});
