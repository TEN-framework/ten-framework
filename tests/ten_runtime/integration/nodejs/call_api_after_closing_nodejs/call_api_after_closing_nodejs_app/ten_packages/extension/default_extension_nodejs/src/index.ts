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

  async onConfigure(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onConfigure");
  }

  async onInit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onInit");
  }

  async onStart(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onStart");

    const testData = Data.Create("testData");
    testData.allocBuf(10);
    let buf = testData.lockBuf();

    let view = new Uint8Array(buf);
    view[0] = 1;
    view[1] = 2;
    view[2] = 3;
    testData.unlockBuf(buf);

    let copiedBuf = testData.getBuf();
    let copiedView = new Uint8Array(copiedBuf);
    assert(copiedView[0] === 1, "copiedView[0] incorrect");
    assert(copiedView[1] === 2, "copiedView[1] incorrect");
    assert(copiedView[2] === 3, "copiedView[2] incorrect");
  }

  async onStop(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onStop");
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    // Create a new promise but not await it
    const promise = new Promise((resolve, reject) => {
      setTimeout(async () => {
        const err = tenEnv.logInfo("Promise done after on deinit done");
        assert(err !== null, "logInfo() should return an error");

        const newCmd = Cmd.Create("test");
        const [_, err1] = await tenEnv.sendCmd(newCmd);
        assert(err1 !== null, "sendCmd() should return an error");

        const newData = Data.Create("testData");
        const err2 = await tenEnv.sendData(newData);
        assert(err2 !== null, "sendData() should return an error");

        const newVideoFrame = VideoFrame.Create("testVideoFrame");
        const err3 = await tenEnv.sendVideoFrame(newVideoFrame);
        assert(err3 !== null, "sendVideoFrame() should return an error");

        const newAudioFrame = AudioFrame.Create("testAudioFrame");
        const err4 = await tenEnv.sendAudioFrame(newAudioFrame);
        assert(err4 !== null, "sendAudioFrame() should return an error");

        const newCmdResult = CmdResult.Create(StatusCode.OK, newCmd);
        const err5 = await tenEnv.returnResult(newCmdResult);
        assert(err5 !== null, "returnResult() should return an error");

        const [propertyJson, err6] = await tenEnv.getPropertyToJson(
          "testProperty"
        );
        assert(err6 !== null, "getPropertyToJson() should return an error");

        const err7 = await tenEnv.setPropertyFromJson(
          "testProperty",
          propertyJson
        );
        assert(err7 !== null, "setPropertyFromJson() should return an error");

        const [result2, err8] = await tenEnv.getPropertyNumber("testProperty");
        assert(err8 !== null, "getPropertyNumber() should return an error");

        const err9 = await tenEnv.setPropertyNumber("testProperty", result2);
        assert(err9 !== null, "setPropertyNumber() should return an error");

        const [result3, err10] = await tenEnv.getPropertyString("testProperty");
        assert(err10 !== null, "getPropertyString() should return an error");

        const err11 = await tenEnv.setPropertyString("testProperty", result3);
        assert(err11 !== null, "setPropertyString() should return an error");

        const err12 = await tenEnv.initPropertyFromJson("testProperty");
        assert(err12 !== null, "initPropertyFromJson() should return an error");

        console.log("promise done");

        resolve("done");
      }, 1000);
    });

    console.log("DefaultExtension onDeinit");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.logDebug("DefaultExtension onCmd");

    const cmdName = cmd.getName();
    tenEnv.logVerbose("cmdName:" + cmdName);

    const testCmd = Cmd.Create("test");
    const [result, _] = await tenEnv.sendCmd(testCmd);
    assert(result !== null, "result is null");

    tenEnv.logInfo(
      "received result detail:" + result?.getPropertyToJson("detail")
    );

    const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
    cmdResult.setPropertyFromJson(
      "detail",
      JSON.stringify({ key1: "value1", key2: 2 })
    );

    const [detailJson, err] = cmdResult.getPropertyToJson("detail");
    tenEnv.logInfo("detailJson:" + detailJson);

    tenEnv.returnResult(cmdResult);
  }
}

@RegisterAddonAsExtension("default_extension_nodejs")
class DefaultExtensionAddon extends Addon {
  async onInit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtensionAddon onInit");
  }

  async onCreateInstance(
    _tenEnv: TenEnv,
    instanceName: string
  ): Promise<Extension> {
    return new DefaultExtension(instanceName);
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtensionAddon onDeinit");
  }
}
