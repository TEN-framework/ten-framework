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
  private originalGraphReceiverExtension = "";
  private originalGraphId = "";

  constructor(name: string) {
    super(name);
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension4 onCmd");

    const cmdName = cmd.getName();
    tenEnv.log(LogLevel.INFO, `Received command: ${cmdName}`);

    if (cmdName === "set_original_graph_info") {
      await this.handleSetOriginalGraphInfoCmd(tenEnv, cmd);
    } else if (cmdName === "A") {
      await this.handleACmd(tenEnv, cmd);
    } else {
      throw new Error(`TestExtension4 received unexpected command: ${cmdName}`);
    }
  }

  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension4 onData");

    const dataName = data.getName();
    tenEnv.log(LogLevel.INFO, `Received data: ${dataName}`);

    if (dataName === "data_to_extension_4") {
      // Send data back to the original graph's test_extension_1
      if (this.originalGraphReceiverExtension && this.originalGraphId) {
        const responseData = Data.Create("data_from_new_graph");
        responseData.setPropertyFromJson(
          "content",
          JSON.stringify({
            from: "test_extension_4",
            original_data: data.getPropertyToJson("content")[0],
          }),
        );

        // Set destination to the original graph's test_extension_1
        responseData.setDests([
          {
            appUri: "",
            graphId: this.originalGraphId,
            extensionName: this.originalGraphReceiverExtension,
          },
        ]);

        const err = await tenEnv.sendData(responseData);
        if (err) {
          throw new Error(`Failed to send data back to original graph: ${err}`);
        }

        tenEnv.log(
          LogLevel.INFO,
          "TestExtension4 sent data back to original graph",
        );
      } else {
        throw new Error("TestExtension4 has not received original graph info");
      }
    } else {
      throw new Error(`TestExtension4 received unexpected data: ${dataName}`);
    }
  }

  private async handleSetOriginalGraphInfoCmd(
    tenEnv: TenEnv,
    cmd: Cmd,
  ): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension4 handleSetOriginalGraphInfoCmd");

    // Store the original graph receiver extension and graph ID
    const [originalGraphReceiverExtension] = cmd.getPropertyString(
      "original_graph_receiver_extension",
    );
    const [originalGraphId] = cmd.getPropertyString("original_graph_id");

    this.originalGraphReceiverExtension = originalGraphReceiverExtension;
    this.originalGraphId = originalGraphId;

    tenEnv.log(
      LogLevel.INFO,
      `Stored original graph info - receiver: ${originalGraphReceiverExtension}, graph_id: ${originalGraphId}`,
    );

    // Return result for the command
    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    tenEnv.returnResult(cmdResult);
  }

  private async handleACmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.log(LogLevel.INFO, "TestExtension4 handleACmd");

    // This command comes directly from test_extension_2
    // Just acknowledge it by returning OK
    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    tenEnv.returnResult(cmdResult);

    // Send data to the original graph's test_extension_1
    const data = Data.Create("data_from_new_graph");
    data.setDests([
      {
        appUri: "",
        graphId: this.originalGraphId,
        extensionName: this.originalGraphReceiverExtension,
      },
    ]);

    const err = await tenEnv.sendData(data);
    if (err) {
      throw new Error(`Failed to send data to original graph: ${err}`);
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
