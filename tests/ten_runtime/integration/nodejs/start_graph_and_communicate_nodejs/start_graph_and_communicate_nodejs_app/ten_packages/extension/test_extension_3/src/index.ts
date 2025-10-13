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

class TestExtension3 extends Extension {
  constructor(name: string) {
    super(name);
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();

    if (cmdName === "B") {
      await this.handleBCmd(tenEnv, cmd);
    } else {
      tenEnv.log(
        LogLevel.ERROR,
        `test_extension_3 received unexpected cmd: ${cmdName}`,
      );
    }
  }

  private async handleBCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    // Simply return OK status for command B
    const result = CmdResult.Create(StatusCode.OK, cmd);
    tenEnv.returnResult(result);
  }
}

@RegisterAddonAsExtension("test_extension_3")
class TestExtension3Addon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new TestExtension3(instanceName);
  }
}
