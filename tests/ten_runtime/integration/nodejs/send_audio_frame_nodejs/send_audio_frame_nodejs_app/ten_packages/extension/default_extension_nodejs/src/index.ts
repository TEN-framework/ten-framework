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
  private cachedCmd: Cmd | null = null;

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
  }

  async onStop(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onStop");
  }

  async onDeinit(_tenEnv: TenEnv): Promise<void> {
    console.log("DefaultExtension onDeinit");
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();
    tenEnv.logDebug("DefaultExtension onCmd " + cmdName);

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
    let buf = audioFrame.lockBuf();
    const bufView = new Uint8Array(buf);
    for (let i = 0; i < 320; i++) {
      bufView[i] = i % 256;
    }
    audioFrame.unlockBuf(buf);

    await tenEnv.sendAudioFrame(audioFrame);
  }

  async onAudioFrame(tenEnv: TenEnv, frame: AudioFrame): Promise<void> {
    tenEnv.logDebug("DefaultExtension onAudioFrame");

    assert(
      frame.getDataFmt() === AudioFrameDataFmt.INTERLEAVE,
      "DataFmt is not INTERLEAVE"
    );
    assert(frame.getBytesPerSample() === 2, "BytesPerSample is not 2");
    assert(frame.getSampleRate() === 16000, "SampleRate is not 16000");
    assert(frame.getNumberOfChannels() === 1, "NumberOfChannels is not 1");
    assert(
      frame.getSamplesPerChannel() === 160,
      "SamplesPerChannel is not 160"
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
    assert(cmd !== null, "Cached cmd is null");

    const result = CmdResult.Create(StatusCode.OK, cmd!);
    result.setPropertyString("detail", "success");
    await tenEnv.returnResult(result);
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
