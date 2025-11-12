//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../ten_addon.js";
import { Msg } from "./msg.js";

export enum AudioFrameDataFmt {
  INTERLEAVE = 1,
  NON_INTERLEAVE = 2,
}

export class AudioFrame extends Msg {
  private constructor(name: string, createShellOnly: boolean) {
    super();

    if (createShellOnly) {
      return;
    }

    ten_addon.ten_nodejs_audio_frame_create(this, name);
  }

  static Create(name: string): AudioFrame {
    return new AudioFrame(name, false);
  }

  allocBuf(size: number): void {
    ten_addon.ten_nodejs_audio_frame_alloc_buf(this, size);
  }

  lockBuf(): ArrayBuffer {
    return ten_addon.ten_nodejs_audio_frame_lock_buf(this);
  }

  unlockBuf(buf: ArrayBuffer): void {
    ten_addon.ten_nodejs_audio_frame_unlock_buf(this, buf);
  }

  getBuf(): ArrayBuffer {
    return ten_addon.ten_nodejs_audio_frame_get_buf(this);
  }

  getTimestamp(): number {
    return ten_addon.ten_nodejs_audio_frame_get_timestamp(this);
  }

  setTimestamp(timestamp: number): void {
    ten_addon.ten_nodejs_audio_frame_set_timestamp(this, timestamp);
  }

  getSampleRate(): number {
    return ten_addon.ten_nodejs_audio_frame_get_sample_rate(this);
  }

  setSampleRate(sampleRate: number): void {
    ten_addon.ten_nodejs_audio_frame_set_sample_rate(this, sampleRate);
  }

  getSamplesPerChannel(): number {
    return ten_addon.ten_nodejs_audio_frame_get_samples_per_channel(this);
  }

  setSamplesPerChannel(samplesPerChannel: number): void {
    ten_addon.ten_nodejs_audio_frame_set_samples_per_channel(
      this,
      samplesPerChannel,
    );
  }

  getBytesPerSample(): number {
    return ten_addon.ten_nodejs_audio_frame_get_bytes_per_sample(this);
  }

  setBytesPerSample(bytesPerSample: number): void {
    ten_addon.ten_nodejs_audio_frame_set_bytes_per_sample(this, bytesPerSample);
  }

  getNumberOfChannels(): number {
    return ten_addon.ten_nodejs_audio_frame_get_number_of_channels(this);
  }

  setNumberOfChannels(numberOfChannels: number): void {
    ten_addon.ten_nodejs_audio_frame_set_number_of_channels(
      this,
      numberOfChannels,
    );
  }

  getDataFmt(): AudioFrameDataFmt {
    return ten_addon.ten_nodejs_audio_frame_get_data_fmt(this);
  }

  setDataFmt(dataFmt: AudioFrameDataFmt): void {
    ten_addon.ten_nodejs_audio_frame_set_data_fmt(this, dataFmt);
  }

  getLineSize(): number {
    return ten_addon.ten_nodejs_audio_frame_get_line_size(this);
  }

  setLineSize(lineSize: number): void {
    ten_addon.ten_nodejs_audio_frame_set_line_size(this, lineSize);
  }

  isEof(): boolean {
    return ten_addon.ten_nodejs_audio_frame_is_eof(this);
  }

  setEof(eof: boolean): void {
    ten_addon.ten_nodejs_audio_frame_set_eof(this, eof);
  }
}

ten_addon.ten_nodejs_audio_frame_register_class(AudioFrame);
