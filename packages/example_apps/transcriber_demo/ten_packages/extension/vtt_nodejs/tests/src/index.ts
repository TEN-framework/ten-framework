//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { ExtensionTester, TenEnvTester } from "ten-runtime-nodejs";

export class MyExtensionTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    tenEnvTester.logInfo("MyExtensionTester onStart");

    new Promise((resolve) => {
      setTimeout(() => {
        resolve(true);
      }, 1000);
    }).then(() => {
      tenEnvTester.stopTest();
    });
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.logInfo("MyExtensionTester onStop");
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.logInfo("MyExtensionTester onDeinit");
  }
}
