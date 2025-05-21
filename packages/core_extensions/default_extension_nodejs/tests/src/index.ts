//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  AddonManager,
  ExtensionTester,
  TenEnvTester,
} from "ten-runtime-nodejs";

class MyExtensionTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    console.log("MyExtensionTester onStart");

    tenEnvTester.stopTest();
  }

  async onStop(tenEnvTester: TenEnvTester) {
    console.log("MyExtensionTester onStop");
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    console.log("MyExtensionTester onDeinit");
  }
}

async function main() {
  const test_addon_name = "default_extension_nodejs";

  await AddonManager.getInstance().loadSingleAddon(test_addon_name);

  console.log("loadAllAddons done");

  const extensionTester = new MyExtensionTester();
  extensionTester.setTestModeSingle(test_addon_name, "{}");
  await extensionTester.run();

  AddonManager.getInstance().deinit();

  console.log("deinit done");
}

main();
