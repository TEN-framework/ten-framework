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
  LogLevel,
  Cmd,
  CmdResult,
  StatusCode,
  VideoFrame,
  Data,
  AudioFrame,
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

    const greetingCmd = Cmd.Create("greeting");

    const [greetingMsg] = await tenEnv.getPropertyString("greetingMsg");

    if (greetingMsg) {
      greetingCmd.setPropertyString("greetingMsg", greetingMsg);
    }

    tenEnv.sendCmd(greetingCmd);
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();

    tenEnv.logInfo("DefaultExtension onCmd" + cmdName);

    if (cmdName === "ping") {
      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      tenEnv.returnResult(cmdResult);

      const pongCmd = Cmd.Create("pong");
      tenEnv.sendCmd(pongCmd);
    } else {
      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "unknown command");
      tenEnv.returnResult(cmdResult);
    }
  }

  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    tenEnv.logInfo("DefaultExtension onData");

    const dataName = data.getName();
    if (dataName === "ping") {
      const pongData = Data.Create("pong");
      tenEnv.sendData(pongData);
    } else {
      tenEnv.log(LogLevel.ERROR, "unknown data received: " + dataName);
    }
  }

  async onVideoFrame(tenEnv: TenEnv, videoFrame: VideoFrame): Promise<void> {
    tenEnv.logInfo("DefaultExtension onVideoFrame");

    const videoFrameName = videoFrame.getName();
    if (videoFrameName === "ping") {
      const pongVideoFrame = VideoFrame.Create("pong");
      tenEnv.sendVideoFrame(pongVideoFrame);
    } else {
      tenEnv.log(
        LogLevel.ERROR,
        "unknown video frame received: " + videoFrameName,
      );
    }
  }

  async onAudioFrame(tenEnv: TenEnv, audioFrame: AudioFrame): Promise<void> {
    tenEnv.logInfo("DefaultExtension onAudioFrame");

    const audioFrameName = audioFrame.getName();
    if (audioFrameName === "ping") {
      const pongAudioFrame = AudioFrame.Create("pong");
      tenEnv.sendAudioFrame(pongAudioFrame);
    } else {
      tenEnv.log(
        LogLevel.ERROR,
        "unknown audio frame received: " + audioFrameName,
      );
    }
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
