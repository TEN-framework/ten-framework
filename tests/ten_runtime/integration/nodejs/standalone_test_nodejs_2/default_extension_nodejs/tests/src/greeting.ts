//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  Cmd,
  CmdResult,
  ExtensionTester,
  StatusCode,
  TenEnvTester,
  TenError,
  TenErrorCode,
} from "ten-runtime-nodejs";

export class GreetingTester extends ExtensionTester {
  private expectedGreetingMsg: string;

  constructor(expectedGreetingMsg: string) {
    super();
    this.expectedGreetingMsg = expectedGreetingMsg;
  }

  async onStart(tenEnvTester: TenEnvTester) {
    tenEnvTester.logInfo("GreetingTester onStart");
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.logInfo("GreetingTester onStop");
  }

  async onCmd(tenEnvTester: TenEnvTester, cmd: Cmd) {
    const cmdName = cmd.getName();
    tenEnvTester.logInfo("GreetingTester onCmd: " + cmdName);

    if (cmdName === "greeting") {
      const [actualGreetingMsg, err] = cmd.getPropertyString("greetingMsg");
      if (err) {
        throw new Error(`Failed to get greeting message: ${err.errorMessage}`);
      }

      if (actualGreetingMsg !== this.expectedGreetingMsg) {
        const err = new TenError(
          TenErrorCode.ErrorCodeGeneric,
          `Expected greeting message: ${this.expectedGreetingMsg}, but got: ${actualGreetingMsg}`,
        );
        tenEnvTester.stopTest(err);
        return;
      }

      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      await tenEnvTester.returnResult(cmdResult);

      tenEnvTester.stopTest();
    }
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.logInfo("GreetingTester onDeinit");
  }
}
