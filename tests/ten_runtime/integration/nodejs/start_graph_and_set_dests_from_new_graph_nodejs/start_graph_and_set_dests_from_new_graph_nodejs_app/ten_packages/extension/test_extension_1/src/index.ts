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
  StartGraphCmd,
  StopGraphCmd,
} from "ten-runtime-nodejs";

function assert(condition: boolean, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

class TestExtension1 extends Extension {
  private startAndStopGraphIsCompleted = false;
  private receivedDataFromNewGraph = false;
  private testCmd: Cmd | null = null;

  constructor(name: string) {
    super(name);
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension1 onStart");

    // Start a new graph
    const startGraphCmd = StartGraphCmd.Create();

    // Set destination to current app (empty string)
    startGraphCmd.setDests([{ appUri: "" }]);

    // Define the new graph JSON
    const graphJSON = {
      nodes: [
        {
          type: "extension",
          name: "test_extension_2",
          addon: "test_extension_2",
        },
        {
          type: "extension",
          name: "test_extension_3",
          addon: "test_extension_3",
        },
        {
          type: "extension",
          name: "test_extension_4",
          addon: "test_extension_4",
        },
      ],
      connections: [
        {
          extension: "test_extension_2",
          cmd: [
            {
              name: "A",
              dest: [
                {
                  extension: "test_extension_3",
                  msg_conversion: {
                    keep_original: true,
                    type: "per_property",
                    rules: [
                      {
                        path: "ten.name",
                        conversion_mode: "fixed_value",
                        value: "B",
                      },
                    ],
                  },
                },
                {
                  extension: "test_extension_4",
                },
              ],
            },
            {
              name: "set_original_graph_info",
              dest: [
                {
                  extension: "test_extension_4",
                },
              ],
            },
          ],
        },
      ],
    };

    startGraphCmd.setGraphFromJSON(JSON.stringify(graphJSON));

    const [startGraphResult, err] = await tenEnv.sendCmd(startGraphCmd);
    if (err) {
      throw new Error(`Failed to start graph: ${err}`);
    }

    assert(startGraphResult !== undefined, "startGraphResult is undefined");
    assert(
      startGraphResult.getStatusCode() === StatusCode.OK,
      "Start graph command failed",
    );

    // Get the graph ID of the newly created graph
    const [newGraphId] = startGraphResult.getPropertyString("graph_id");

    // Send a 'set_original_graph_info' command to the specified extension
    // in the newly created graph
    const cmdSetOriginalGraphInfo = Cmd.Create("set_original_graph_info");
    cmdSetOriginalGraphInfo.setPropertyString(
      "original_graph_receiver_extension",
      "test_extension_1",
    );

    // Set destination to test_extension_2 in the new graph
    cmdSetOriginalGraphInfo.setDests([
      { appUri: "", graphId: newGraphId, extensionName: "test_extension_2" },
    ]);

    const [, setInfoErr] = await tenEnv.sendCmd(cmdSetOriginalGraphInfo);
    if (setInfoErr) {
      throw new Error(`Failed to set original graph info: ${setInfoErr}`);
    }

    // Send start command to test_extension_2
    const cmdStart = Cmd.Create("start");
    cmdStart.setDests([
      { appUri: "", graphId: newGraphId, extensionName: "test_extension_2" },
    ]);

    const [, startErr] = await tenEnv.sendCmd(cmdStart);
    if (startErr) {
      throw new Error(`Failed to send start command: ${startErr}`);
    }

    // Stop the graph after processing
    const stopGraphCmd = StopGraphCmd.Create();
    stopGraphCmd.setDests([{ appUri: "", graphId: "", extensionName: "" }]);
    stopGraphCmd.setGraphId(newGraphId);

    const [, stopErr] = await tenEnv.sendCmd(stopGraphCmd);
    if (stopErr) {
      throw new Error(`Failed to stop graph: ${stopErr}`);
    }

    this.startAndStopGraphIsCompleted = true;

    if (this.testCmd !== null && this.receivedDataFromNewGraph) {
      this.replyToClient(tenEnv);
    }

    tenEnv.log(LogLevel.INFO, "TestExtension1 onStart completed");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension1 onCmd");

    const cmdName = cmd.getName();
    tenEnv.log(LogLevel.INFO, `Received command: ${cmdName}`);

    if (cmdName === "test") {
      this.testCmd = cmd;

      if (this.startAndStopGraphIsCompleted && this.receivedDataFromNewGraph) {
        this.replyToClient(tenEnv);
      }
    } else {
      throw new Error(`TestExtension1 received unexpected command: ${cmdName}`);
    }
  }

  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension1 onData");

    const dataName = data.getName();
    tenEnv.log(LogLevel.INFO, `Received data: ${dataName}`);

    if (dataName === "data_from_new_graph") {
      this.receivedDataFromNewGraph = true;

      if (this.testCmd !== null && this.startAndStopGraphIsCompleted) {
        this.replyToClient(tenEnv);
      }
    } else {
      throw new Error(`TestExtension1 received unexpected data: ${dataName}`);
    }
  }

  private replyToClient(tenEnv: TenEnv): void {
    tenEnv.log(LogLevel.INFO, "TestExtension1 replyToClient");

    if (this.testCmd === null) {
      throw new Error("testCmd is null when trying to reply");
    }

    const cmdResult = CmdResult.Create(StatusCode.OK, this.testCmd);
    cmdResult.setPropertyString("detail", JSON.stringify({ id: 1, name: "a" }));

    tenEnv.returnResult(cmdResult);
    this.testCmd = null;
  }
}

@RegisterAddonAsExtension("test_extension_1")
class TestExtension1Addon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new TestExtension1(instanceName);
  }
}
