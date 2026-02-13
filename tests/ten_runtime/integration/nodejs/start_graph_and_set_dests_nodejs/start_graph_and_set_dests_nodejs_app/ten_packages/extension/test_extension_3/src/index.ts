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
  Data,
  Addon,
  RegisterAddonAsExtension,
  LogLevel,
} from "ten-runtime-nodejs";

class TestExtension3 extends Extension {
  constructor(name: string) {
    super(name);
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension3 onCmd");

    const cmdName = cmd.getName();
    tenEnv.log(LogLevel.INFO, `Received command: ${cmdName}`);

    if (cmdName === "B") {
      // This command comes from test_extension_2's "A" command after
      // msg_conversion
      tenEnv.log(
        LogLevel.INFO,
        "TestExtension3 received B command (converted from A)",
      );

      // Return result for the B command
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      tenEnv.returnResult(cmdResult);
    } else {
      throw new Error(`TestExtension3 received unexpected command: ${cmdName}`);
    }
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
