//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  Extension,
  TenEnv,
  Cmd,
  CmdResult,
  StatusCode,
  Addon,
  RegisterAddonAsExtension,
  LogLevel,
} from "ten-runtime-nodejs";

class TestExtension2 extends Extension {
  constructor(name: string) {
    super(name);
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();

    if (cmdName === "start") {
      await this.handleStartCmd(tenEnv, cmd);
    } else {
      tenEnv.log(
        LogLevel.ERROR,
        `test_extension_2 received unexpected cmd: ${cmdName}`,
      );
    }
  }

  private async handleStartCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdA = Cmd.Create("A");

    const [, err] = await tenEnv.sendCmd(cmdA);
    if (err) {
      throw new Error(`Failed to send A command: ${err}`);
    }

    // Return result for the start command
    const result = CmdResult.Create(StatusCode.OK, cmd);
    tenEnv.returnResult(result);
  }
}

@RegisterAddonAsExtension("test_extension_2")
class TestExtension2Addon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new TestExtension2(instanceName);
  }
}
