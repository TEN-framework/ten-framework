//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import type { Cmd } from "./msg/cmd/cmd.js";
import type { Data } from "./msg/data.js";
import type { AudioFrame } from "./msg/audio_frame.js";
import type { VideoFrame } from "./msg/video_frame.js";
import type { TenEnv } from "./ten_env.js";
import ten_addon from "./ten_addon.js";

export abstract class Extension {
  constructor(name: string) {
    ten_addon.ten_nodejs_extension_create(this, name);
  }

  private async onConfigureProxy(tenEnv: TenEnv): Promise<void> {
    await this.onConfigure(tenEnv);

    ten_addon.ten_nodejs_ten_env_on_configure_done(tenEnv);
  }

  private async onInitProxy(tenEnv: TenEnv): Promise<void> {
    await this.onInit(tenEnv);

    ten_addon.ten_nodejs_ten_env_on_init_done(tenEnv);
  }

  private async onStartProxy(tenEnv: TenEnv): Promise<void> {
    await this.onStart(tenEnv);

    ten_addon.ten_nodejs_ten_env_on_start_done(tenEnv);
  }

  private async onStopProxy(tenEnv: TenEnv): Promise<void> {
    await this.onStop(tenEnv);

    ten_addon.ten_nodejs_ten_env_on_stop_done(tenEnv);
  }

  private async onDeinitProxy(tenEnv: TenEnv): Promise<void> {
    await this.onDeinit(tenEnv);

    ten_addon.ten_nodejs_ten_env_on_deinit_done(tenEnv);
  }

  private async onCmdProxy(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    await this.onCmd(tenEnv, cmd);
  }

  private async onDataProxy(tenEnv: TenEnv, data: Data): Promise<void> {
    await this.onData(tenEnv, data);
  }

  private async onAudioFrameProxy(
    tenEnv: TenEnv,
    frame: AudioFrame,
  ): Promise<void> {
    await this.onAudioFrame(tenEnv, frame);
  }

  private async onVideoFrameProxy(
    tenEnv: TenEnv,
    frame: VideoFrame,
  ): Promise<void> {
    await this.onVideoFrame(tenEnv, frame);
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onConfigure(tenEnv: TenEnv): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onInit(tenEnv: TenEnv): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onStart(tenEnv: TenEnv): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onStop(tenEnv: TenEnv): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onDeinit(tenEnv: TenEnv): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onAudioFrame(tenEnv: TenEnv, frame: AudioFrame): Promise<void> {
    // stub for override
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async onVideoFrame(tenEnv: TenEnv, frame: VideoFrame): Promise<void> {
    // stub for override
  }
}
