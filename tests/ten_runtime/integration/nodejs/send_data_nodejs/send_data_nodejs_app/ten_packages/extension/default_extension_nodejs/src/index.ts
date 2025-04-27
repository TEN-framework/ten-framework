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
} from "ten-runtime-nodejs";

function assert(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(message);
  }
}

class DefaultExtension extends Extension {
  // Cache the received cmd
  private cachedCmd: Cmd | null = null;
  private dataRespRecvCount: number = 0;

  constructor(name: string) {
    super(name);
  }

  async onConfigure(tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onConfigure");

    await tenEnv.initPropertyFromJson(
      JSON.stringify({ key1: "value1", key2: 2 })
    );
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onInit");

    const [value1, err] = await tenEnv.getPropertyString("key1");
    assert(err == null, "err is not null");

    const [value2, err2] = await tenEnv.getPropertyNumber("key2");
    assert(err2 == null, "err2 is not null");

    console.log("value1:", value1);
    console.log("value2:", value2);

    assert(value1 === "value1", "value1 incorrect");
    assert(value2 === 2, "value2 incorrect");
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

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onDeinit");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    tenEnv.logDebug("DefaultExtension onCmd");

    const cmdName = cmd.getName();
    tenEnv.logVerbose("cmdName:" + cmdName);

    const newData = Data.Create("data");

    const str = "Hello, World!";

    const encoder = new TextEncoder();
    const uint8Array = encoder.encode(str);

    newData.allocBuf(uint8Array.byteLength);
    let dataBuf = newData.lockBuf();
    let dataBufView = new Uint8Array(dataBuf);
    dataBufView.set(uint8Array);

    newData.unlockBuf(dataBuf);

    // This data is sent to simple_echo_cpp extension, and this cpp extension
    // will simply echo the data back.
    await tenEnv.sendData(newData);

    const data2 = Data.Create("data2");
    data2.setPropertyString("key1", "value1");
    data2.setPropertyNumber("key2", 2);
    data2.setPropertyBool("key3", true);

    const valueBuf = new ArrayBuffer(3);
    const uint8View = new Uint8Array(valueBuf);
    uint8View[0] = 1;
    uint8View[1] = 2;
    uint8View[2] = 3;
    data2.setPropertyBuf("key4", valueBuf);

    // This data is sent to default_extension_nodejs extension, and this
    // extension will check the data and return a 'data2_return' data back.
    await tenEnv.sendData(data2);

    this.cachedCmd = cmd;
  }

  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    tenEnv.logDebug("DefaultExtension onData name:" + data.getName());

    if (data.getName() === "data2") {
      const [value1, err] = data.getPropertyString("key1");
      assert(err == null, "err is not null");

      const [value2, err2] = data.getPropertyNumber("key2");
      assert(err2 == null, "err2 is not null");

      const [value3, err3] = data.getPropertyBool("key3");
      assert(err3 == null, "err3 is not null");

      const [value4, err4] = data.getPropertyBuf("key4");
      assert(err4 == null, "err4 is not null");

      assert(value1 === "value1", "value1 incorrect");
      assert(value2 === 2, "value2 incorrect");
      assert(value3 === true, "value3 incorrect");

      const value4Uint8Array = new Uint8Array(value4);
      assert(value4Uint8Array[0] === 1, "value4[0] incorrect");
      assert(value4Uint8Array[1] === 2, "value4[1] incorrect");
      assert(value4Uint8Array[2] === 3, "value4[2] incorrect");

      const data2Return = Data.Create("data2_return");
      await tenEnv.sendData(data2Return);
    } else if (data.getName() === "data") {
      const buf = data.getBuf();
      const bufView = new Uint8Array(buf);

      for (let i = 0; i < buf.byteLength; i++) {
        // Since this is a buffer copied through getBuf, modifications to it
        // will _not_ affect the actual data buffer.
        bufView[i] += 1;
      }

      let lockedBuf = data.lockBuf();
      const decoder = new TextDecoder();
      const decodedStr = decoder.decode(lockedBuf);

      assert(decodedStr === "Hello, World!", "view incorrect");

      data.unlockBuf(lockedBuf);

      this.dataRespRecvCount++;
    } else if (data.getName() === "data2_return") {
      this.dataRespRecvCount++;
    }

    if (this.dataRespRecvCount === 2) {
      const cmdResult = CmdResult.Create(StatusCode.OK, this.cachedCmd!);
      cmdResult.setPropertyString("detail", "success");
      await tenEnv.returnResult(cmdResult);
    }
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
