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

class TestExtension4 extends Extension {
  constructor(name: string) {
    super(name);
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();

    if (cmdName === "A") {
      await this.handleACmd(tenEnv, cmd);
    } else {
      tenEnv.log(
        LogLevel.ERROR,
        `test_extension_4 received unexpected cmd: ${cmdName}`,
      );
    }
  }

  private async handleACmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    // Return OK result for command A
    const result = CmdResult.Create(StatusCode.OK, cmd);
    tenEnv.returnResult(result);

    // Send data back to the original graph
    const data = Data.Create("data_from_new_graph");

    const err = await tenEnv.sendData(data);
    if (err) {
      tenEnv.log(LogLevel.ERROR, `Failed to send data: ${err}`);
    }
  }
}

@RegisterAddonAsExtension("test_extension_4")
class TestExtension4Addon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new TestExtension4(instanceName);
  }
}

