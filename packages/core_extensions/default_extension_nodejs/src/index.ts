//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
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

class DefaultExtension extends Extension {
  constructor(name: string) {
    super(name);
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onConfigure");
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onInit");
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onStart");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.logInfo("DefaultExtension onCmd", cmd.getName());

    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    cmdResult.setPropertyString("detail", "This is a demo");
    tenEnv.returnResult(cmdResult);
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onStop");
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onDeinit");
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
