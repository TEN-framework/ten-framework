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
  LogLevel,
  Cmd,
  Data,
  CmdResult,
  StatusCode,
  VideoFrame,
  AudioFrame,
} from "ten-runtime-nodejs";

function assert(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(message);
  }
}

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

    const testData = Data.Create("testData");
    testData.allocBuf(10);
    const buf = testData.lockBuf();

    const view = new Uint8Array(buf);
    view[0] = 1;
    view[1] = 2;
    view[2] = 3;
    testData.unlockBuf(buf);

    const copiedBuf = testData.getBuf();
    const copiedView = new Uint8Array(copiedBuf);
    assert(copiedView[0] === 1, "copiedView[0] incorrect");
    assert(copiedView[1] === 2, "copiedView[1] incorrect");
    assert(copiedView[2] === 3, "copiedView[2] incorrect");
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onStop");
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    // Create a new promise but not await it
    const promise = new Promise((resolve, reject) => {
      setTimeout(async () => {
        const err = tenEnv.logInfo("Promise done after on deinit done");
        assert(err !== undefined, "log() should return an error");

        const newCmd = Cmd.Create("test");
        const [_, err1] = await tenEnv.sendCmd(newCmd);
        assert(err1 !== undefined, "sendCmd() should return an error");

        const newData = Data.Create("testData");
        const err2 = await tenEnv.sendData(newData);
        assert(err2 !== undefined, "sendData() should return an error");

        const newVideoFrame = VideoFrame.Create("testVideoFrame");
        const err3 = await tenEnv.sendVideoFrame(newVideoFrame);
        assert(err3 !== undefined, "sendVideoFrame() should return an error");

        const newAudioFrame = AudioFrame.Create("testAudioFrame");
        const err4 = await tenEnv.sendAudioFrame(newAudioFrame);
        assert(err4 !== undefined, "sendAudioFrame() should return an error");

        const newCmdResult = CmdResult.Create(StatusCode.OK, newCmd);
        const err5 = await tenEnv.returnResult(newCmdResult);
        assert(err5 !== undefined, "returnResult() should return an error");

        const [propertyJson, err6] =
          await tenEnv.getPropertyToJson("testProperty");
        assert(
          err6 !== undefined,
          "getPropertyToJson() should return an error",
        );

        const err7 = await tenEnv.setPropertyFromJson(
          "testProperty",
          propertyJson,
        );
        assert(
          err7 !== undefined,
          "setPropertyFromJson() should return an error",
        );

        const [result2, err8] = await tenEnv.getPropertyNumber("testProperty");
        assert(
          err8 !== undefined,
          "getPropertyNumber() should return an error",
        );

        const err9 = await tenEnv.setPropertyNumber("testProperty", result2);
        assert(
          err9 !== undefined,
          "setPropertyNumber() should return an error",
        );

        const [result3, err10] = await tenEnv.getPropertyString("testProperty");
        assert(
          err10 !== undefined,
          "getPropertyString() should return an error",
        );

        const err11 = await tenEnv.setPropertyString("testProperty", result3);
        assert(
          err11 !== undefined,
          "setPropertyString() should return an error",
        );

        const err12 = await tenEnv.initPropertyFromJson("testProperty");
        assert(
          err12 !== undefined,
          "initPropertyFromJson() should return an error",
        );

        tenEnv.logInfo("promise done");

        resolve("done");
      }, 1000);
    });

    tenEnv.logInfo("DefaultExtension onDeinit");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.logInfo("DefaultExtension onCmd");

    const cmdName = cmd.getName();
    tenEnv.logInfo("cmdName:" + cmdName);

    const testCmd = Cmd.Create("test");
    const [result, _] = await tenEnv.sendCmd(testCmd);
    assert(result !== undefined, "result is undefined");

    tenEnv.logInfo(
      "received result detail:" + result?.getPropertyToJson("detail"),
    );

    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    cmdResult.setPropertyFromJson(
      "detail",
      JSON.stringify({ key1: "value1", key2: 2 }),
    );

    const [detailJson, err] = cmdResult.getPropertyToJson("detail");
    tenEnv.logInfo("detailJson:" + detailJson);

    tenEnv.returnResult(cmdResult);
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
