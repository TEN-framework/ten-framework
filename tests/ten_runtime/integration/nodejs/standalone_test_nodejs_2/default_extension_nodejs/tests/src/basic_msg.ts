//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  AudioFrame,
  Cmd,
  CmdResult,
  Data,
  ExtensionTester,
  LogLevel,
  StatusCode,
  TenEnvTester,
  VideoFrame,
} from "ten-runtime-nodejs";

export class CmdTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "CmdTester onStart");

    const pingCmd = Cmd.Create("ping");
    tenEnvTester.sendCmd(pingCmd);
  }

  async onCmd(tenEnvTester: TenEnvTester, cmd: Cmd) {
    const cmdName = cmd.getName();
    tenEnvTester.logInfo("CmdTester onCmd: " + cmdName);

    if (cmdName === "pong") {
      tenEnvTester.log(LogLevel.INFO, "pong cmd received");

      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      await tenEnvTester.returnResult(cmdResult);

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "CmdTester onStop");
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "CmdTester onDeinit");
  }
}

export class DataTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "DataTester onStart");

    const pingData = Data.Create("ping");
    tenEnvTester.sendData(pingData);
  }

  async onData(tenEnvTester: TenEnvTester, data: Data) {
    const dataName = data.getName();
    tenEnvTester.logInfo("DataTester onData: " + dataName);

    if (dataName === "pong") {
      tenEnvTester.log(LogLevel.INFO, "pong data received");

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "DataTester onStop");
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "DataTester onDeinit");
  }
}

export class VideoFrameTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "VideoFrameTester onStart");

    const pingVideoFrame = VideoFrame.Create("ping");
    tenEnvTester.sendVideoFrame(pingVideoFrame);
  }

  async onVideoFrame(tenEnvTester: TenEnvTester, videoFrame: VideoFrame) {
    const videoFrameName = videoFrame.getName();
    tenEnvTester.logInfo("VideoFrameTester onVideoFrame: " + videoFrameName);

    if (videoFrameName === "pong") {
      tenEnvTester.log(LogLevel.INFO, "pong video frame received");

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "VideoFrameTester onStop");
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "VideoFrameTester onDeinit");
  }
}

export class AudioFrameTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "AudioFrameTester onStart");

    const pingAudioFrame = AudioFrame.Create("ping");
    tenEnvTester.sendAudioFrame(pingAudioFrame);
  }

  async onAudioFrame(tenEnvTester: TenEnvTester, audioFrame: AudioFrame) {
    const audioFrameName = audioFrame.getName();
    tenEnvTester.logInfo("AudioFrameTester onAudioFrame: " + audioFrameName);

    if (audioFrameName === "pong") {
      tenEnvTester.log(LogLevel.INFO, "pong audio frame received");

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "AudioFrameTester onStop");
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "AudioFrameTester onDeinit");
  }
}

export class TimeoutTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "TimeoutTester onStart");
  }
}
