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
  LogLevel,
  CmdResult,
  StatusCode,
  AudioFrame,
  AudioFrameDataFmt,
} from "ten-runtime-nodejs";

function assert(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(message);
  }
}

class DefaultExtension extends Extension {
  // Cache the received cmd.
  private cachedCmd: Cmd | undefined = undefined;

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

  async onStop(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onStop");
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    tenEnv.logInfo("DefaultExtension onDeinit");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();
    tenEnv.logInfo("DefaultExtension onCmd " + cmdName);

    this.cachedCmd = cmd;

    const audioFrame = AudioFrame.Create("audio_frame");
    audioFrame.setDataFmt(AudioFrameDataFmt.INTERLEAVE);
    audioFrame.setBytesPerSample(2);
    audioFrame.setSampleRate(16000);
    audioFrame.setNumberOfChannels(1);
    audioFrame.setSamplesPerChannel(160);

    const timestamp = Date.now();
    audioFrame.setTimestamp(timestamp);

    audioFrame.setEof(false);
    audioFrame.setLineSize(320);

    audioFrame.allocBuf(320);
    const buf = audioFrame.lockBuf();
    const bufView = new Uint8Array(buf);
    for (let i = 0; i < 320; i++) {
      bufView[i] = i % 256;
    }
    audioFrame.unlockBuf(buf);

    await tenEnv.sendAudioFrame(audioFrame);
  }

  async onAudioFrame(tenEnv: TenEnv, frame: AudioFrame): Promise<void> {
    tenEnv.logInfo("DefaultExtension onAudioFrame");

    assert(
      frame.getDataFmt() === AudioFrameDataFmt.INTERLEAVE,
      "DataFmt is not INTERLEAVE",
    );
    assert(frame.getBytesPerSample() === 2, "BytesPerSample is not 2");
    assert(frame.getSampleRate() === 16000, "SampleRate is not 16000");
    assert(frame.getNumberOfChannels() === 1, "NumberOfChannels is not 1");
    assert(
      frame.getSamplesPerChannel() === 160,
      "SamplesPerChannel is not 160",
    );

    const timestamp = frame.getTimestamp();
    assert(timestamp > 0, "Timestamp is not greater than 0");
    const now = Date.now();
    assert(timestamp <= now, "Timestamp is not less than or equal to now");

    assert(frame.isEof() === false, "isEof is not false");
    assert(frame.getLineSize() === 320, "LineSize is not 320");

    const buf = frame.getBuf();
    const bufView = new Uint8Array(buf);
    for (let i = 0; i < 320; i++) {
      assert(bufView[i] === i % 256, "Data is not correct");
    }

    const cmd = this.cachedCmd;
    assert(cmd !== undefined, "Cached cmd is undefined");

    const result = CmdResult.Create(StatusCode.OK, cmd!);
    result.setPropertyString("detail", "success");
    await tenEnv.returnResult(result);
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
