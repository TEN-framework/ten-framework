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
  TriggerLifeCycleCmd,
} from "ten-runtime-nodejs";

function assert(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(message);
  }
}

class MainExtension extends Extension {
  async onInit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("MainExtension onInit");

    await new Promise(resolve => setTimeout(resolve, 1000));

    // Send cmd to biz extension to check if it is started.
    const cmd = Cmd.Create("check_start");
    cmd.setDests([
      {
        extensionName: "biz",
      },
    ]);
    const [result, _] = await tenEnv.sendCmd(cmd);
    assert(result !== undefined, "result is undefined");

    const [started, err] = result!.getPropertyBool("started");
    assert(!started, "biz extension should not be started as it has been set manual trigger life cycle");

    // Send manual trigger life cycle cmd to biz extension.
    const lifeCycleCmd = TriggerLifeCycleCmd.Create();
    lifeCycleCmd.setStage("start");
    lifeCycleCmd.setDests([
      {
        appUri: "",
        graphId: "",
        extensionName: "biz",
      },
    ]);
    const [triggerLifeCycleResult, triggerLifeCycleErr] = await tenEnv.sendCmd(lifeCycleCmd);
    assert(triggerLifeCycleResult !== undefined, "triggerLifeCycleResult is undefined");
    assert(triggerLifeCycleResult?.getStatusCode() === StatusCode.OK, "triggerLifeCycleResult status code is not OK");

    // Send cmd to biz extension to check if it is started.
    const checkStartCmd = Cmd.Create("check_start");
    checkStartCmd.setDests([
      {
        appUri: "",
        graphId: "",
        extensionName: "biz",
      },
    ]);
    const [checkStartResult, checkStartErr] = await tenEnv.sendCmd(checkStartCmd);
    assert(checkStartResult !== undefined, "checkStartResult is undefined");
    assert(checkStartResult?.getStatusCode() === StatusCode.OK, "checkStartResult status code is not OK");
    assert(checkStartResult?.getPropertyBool("started")[0] === true, "biz extension should be started as it has received manual trigger life cycle cmd");
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("MainExtension onStop");

    await new Promise(resolve => setTimeout(resolve, 1000));

    // Send cmd to biz extension to check if it is stopped.
    const checkStopCmd = Cmd.Create("check_stop");
    checkStopCmd.setDests([
      {
        extensionName: "biz",
      },
    ]);
    const [checkStopResult, checkStopErr] = await tenEnv.sendCmd(checkStopCmd);
    assert(checkStopResult !== undefined, "checkStopResult is undefined");
    assert(checkStopResult?.getStatusCode() === StatusCode.OK, "checkStopResult status code is not OK");
    assert(checkStopResult?.getPropertyBool("stopped")[0] === false, "biz extension should not be stopped as it has been set manual trigger life cycle");

    // Send manual trigger life cycle cmd to biz extension.
    const lifeCycleCmd = TriggerLifeCycleCmd.Create();
    lifeCycleCmd.setStage("stop");
    lifeCycleCmd.setDests([
      {
        appUri: "",
        graphId: "",
        extensionName: "biz",
      },
    ]);
    const [triggerLifeCycleResult, triggerLifeCycleErr] = await tenEnv.sendCmd(lifeCycleCmd);
    assert(triggerLifeCycleResult !== undefined, "triggerLifeCycleResult is undefined");
    assert(triggerLifeCycleResult?.getStatusCode() === StatusCode.OK, "triggerLifeCycleResult status code is not OK");
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("MainExtension onDeinit");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();
    tenEnv.logInfo("cmdName:" + cmdName);

    if (cmdName === "test") {
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      cmdResult.setPropertyString("detail", "ok");
      tenEnv.returnResult(cmdResult);
    } else {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "unknown command");
      tenEnv.returnResult(cmdResult);
    }
  }
}

class BizExtension extends Extension {
  private started: boolean = false;
  private stopped: boolean = false;

  constructor(name: string) {
    super(name);
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    this.started = true;
    tenEnv.logInfo("BizExtension onStart");
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    this.stopped = true;
    tenEnv.logInfo("BizExtension onStop");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();
    tenEnv.logInfo("cmdName:" + cmdName);

    if (cmdName === "check_start") {
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      cmdResult.setPropertyBool("started", this.started);
      tenEnv.returnResult(cmdResult);
    } else if (cmdName === "check_stop") {
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      cmdResult.setPropertyBool("stopped", this.stopped);
      tenEnv.returnResult(cmdResult);
    } else {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "unknown command");
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
    if (instanceName === "main") {
      return new MainExtension(instanceName);
    } else {
      return new BizExtension(instanceName);
    }
  }
}
