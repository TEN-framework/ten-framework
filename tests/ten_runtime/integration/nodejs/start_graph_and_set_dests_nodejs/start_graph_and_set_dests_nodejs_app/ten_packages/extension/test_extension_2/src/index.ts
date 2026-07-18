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
    tenEnv.log(LogLevel.INFO, "TestExtension2 onCmd");

    const cmdName = cmd.getName();
    tenEnv.log(LogLevel.INFO, `Received command: ${cmdName}`);

    if (cmdName === "set_original_graph_info") {
      await this.handleSetOriginalGraphInfoCmd(tenEnv, cmd);
    } else if (cmdName === "start") {
      await this.handleStartCmd(tenEnv, cmd);
    } else {
      throw new Error(`TestExtension2 received unexpected command: ${cmdName}`);
    }
  }

  private async handleSetOriginalGraphInfoCmd(
    tenEnv: TenEnv,
    cmd: Cmd,
  ): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension2 handleSetOriginalGraphInfoCmd");

    // Get the original graph receiver extension
    const [originalGraphReceiverExtension] = cmd.getPropertyString(
      "original_graph_receiver_extension",
    );

    // Get the source location to extract the original graph ID
    const srcLoc = cmd.getSource();
    const originalGraphId = srcLoc.graphId || "";

    // Forward the command with original graph information
    const cmdSetOriginalGraphInfo = Cmd.Create("set_original_graph_info");
    cmdSetOriginalGraphInfo.setPropertyString(
      "original_graph_receiver_extension",
      originalGraphReceiverExtension,
    );
    cmdSetOriginalGraphInfo.setPropertyString(
      "original_graph_id",
      originalGraphId,
    );

    const [, err] = await tenEnv.sendCmd(cmdSetOriginalGraphInfo);
    if (err) {
      throw new Error(
        `Failed to forward set_original_graph_info command: ${err}`,
      );
    }

    // Return result for the original command
    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    tenEnv.returnResult(cmdResult);
  }

  private async handleStartCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension2 handleStartCmd");

    // Send command "A" which will be forwarded to test_extension_3 and
    // test_extension_4 based on the graph connections configuration
    const cmdA = Cmd.Create("A");

    const [, err] = await tenEnv.sendCmd(cmdA);
    if (err) {
      throw new Error(`Failed to send A command: ${err}`);
    }

    // Return result for the start command
    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    tenEnv.returnResult(cmdResult);
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
