//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  Addon,
  RegisterAddonAsExtension,
  Extension,
  TenEnv,
  Cmd,
  CmdResult,
  StatusCode,
} from "ten-runtime-nodejs";

function assert(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(message);
  }
}

class DefaultExtension extends Extension {
  private send_cmd_on_start: boolean;
  private recv_result_count: number;

  constructor(name: string) {
    super(name);
    this.send_cmd_on_start = false;
    this.recv_result_count = 0;
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onInit");

    const [propertyJson, err] = await tenEnv.getPropertyToJson("");
    assert(err === undefined, "err is not undefined");

    // convert propertyJson to object
    const property = JSON.parse(propertyJson);

    // check if property has send_cmd_on_start
    if (property.send_cmd_on_start) {
      this.send_cmd_on_start = true;
    }
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onStart");

    if (this.send_cmd_on_start) {
      const cmd = Cmd.Create("test");

      for await (const [result, error] of tenEnv.sendCmdEx(cmd)) {
        assert(error === undefined, "error is not undefined");
        assert(result !== undefined, "result is undefined");

        const [detail, err] = result!.getPropertyString("detail");
        assert(err === undefined, "err is not undefined");

        tenEnv.logInfo(`detail:${detail}`);
        this.recv_result_count++;
      }

      tenEnv.logInfo(`recv_result_count:${this.recv_result_count}`);

      // close_app
      const closeAppCmd = Cmd.Create("ten:close_app");
      closeAppCmd.setDests([{ appUri: "" }]);
      tenEnv.sendCmd(closeAppCmd);
    }
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onStop");
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onDeinit");

    if (this.send_cmd_on_start) {
      tenEnv.logInfo(
        "recv_result_count is not 10, recv_result_count:" +
          this.recv_result_count,
      );
      assert(this.recv_result_count === 10, "recv_result_count is not 10");
    }
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();
    tenEnv.logInfo(`cmdName:${cmdName}`);

    if (cmdName === "test") {
      // Streaming return result

      for (let i = 0; i < 10; i++) {
        await new Promise((resolve) => setTimeout(resolve, 200));

        const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
        cmdResult.setPropertyString("detail", `nbnb ${i}`);
        if (i !== 9) {
          cmdResult.setFinal(false);
        } else {
          cmdResult.setFinal(true);
        }
        tenEnv.returnResult(cmdResult);
      }
    } else {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      tenEnv.returnResult(cmdResult);
    }
  }
}

@RegisterAddonAsExtension("default_extension_nodejs")
class DefaultExtensionAddon extends Addon {
  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string,
  ): Promise<Extension> {
    return new DefaultExtension(instanceName);
  }
}
