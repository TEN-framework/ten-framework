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

  async onInit(tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onInit");

    const aaaExist = await tenEnv.isPropertyExist("aaa");
    const bbbExist = await tenEnv.isPropertyExist("bbb");
    assert(aaaExist, "aaa not exist");
    assert(!bbbExist, "bbb exist");

    let err: Error | null = null;
    let intValue: number;
    let floatValue: number;
    let nonExistNumValue: number;
    let stringValue: string;
    let propertyJsonStr: string;
    let setIntValue: number;
    let setFloatValue: number;
    let setStringValue: string;
    let setPropertyJsonStr: string;

    [intValue, err] = await tenEnv.getPropertyNumber("keyInt");
    assert(intValue === -32141, "intValue incorrect");

    [floatValue, err] = await tenEnv.getPropertyNumber("keyFloat");
    assert(floatValue > 3.14 && floatValue < 3.15, "floatValue incorrect");

    [nonExistNumValue, err] = await tenEnv.getPropertyNumber("nonExistNumKey");
    assert(err != null, "err is null");

    [stringValue, err] = await tenEnv.getPropertyString("keyString");
    assert(stringValue === "hello", "stringValue incorrect");

    [, err] = await tenEnv.getPropertyString("nonExistStringKey");
    assert(err != null, "err is null");

    [propertyJsonStr, err] = await tenEnv.getPropertyToJson("keyObject");
    const propertyJson = JSON.parse(propertyJsonStr);
    assert(propertyJson.key1 === "value1", "propertyJson incorrect");
    assert(propertyJson.key2 === 2, "propertyJson incorrect");

    [, err] = await tenEnv.getPropertyToJson("nonExistObjectKey");
    assert(err != null, "err is null");

    err = await tenEnv.setPropertyNumber("setKeyInt", 12345);
    assert(err == null, "err is not null");

    err = await tenEnv.setPropertyNumber("setKeyFloat", 3.1415);
    assert(err == null, "err is not null");

    err = await tenEnv.setPropertyString("setKeyString", "happy");
    assert(err == null, "err is not null");

    err = await tenEnv.setPropertyFromJson(
      "setKeyObject",
      JSON.stringify({ key1: "value1", key2: 2 })
    );
    assert(err == null, "err is not null");

    [setIntValue, err] = await tenEnv.getPropertyNumber("setKeyInt");
    assert(setIntValue === 12345, "setIntValue incorrect");

    [setFloatValue, err] = await tenEnv.getPropertyNumber("setKeyFloat");
    assert(
      setFloatValue > 3.1414 && setFloatValue < 3.1416,
      "setFloatValue incorrect"
    );

    [setStringValue, err] = await tenEnv.getPropertyString("setKeyString");
    assert(setStringValue === "happy", "setStringValue incorrect");

    [setPropertyJsonStr, err] = await tenEnv.getPropertyToJson("setKeyObject");
    const setPropertyJson = JSON.parse(setPropertyJsonStr);
    assert(setPropertyJson.key1 === "value1", "setPropertyJson incorrect");
    assert(setPropertyJson.key2 === 2, "setPropertyJson incorrect");

    const cmd = Cmd.Create("hello");
    const cmdName = cmd.getName();
    assert(cmdName === "hello", "cmdName incorrect");
  }

  async onStart(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onStart");
  }

  async onStop(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onStop");
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onDeinit");
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
