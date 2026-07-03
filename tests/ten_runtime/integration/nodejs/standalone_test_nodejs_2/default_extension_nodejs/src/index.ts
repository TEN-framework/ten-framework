//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import assert from "assert";
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
    const err1 = tenEnv.logInfo("DefaultExtension onConfigure");
    assert(!err1, `Failed to log onConfigure: ${err1?.errorMessage}`);

    // Test fields with plain object
    const err2 = tenEnv.logInfo("Test fields with plain object", "test", {
      stringField: "test string",
      numberField: 123,
      booleanField: true,
      arrayField: [1, 2, 3],
      nestedObject: { key: "value" },
    });
    assert(!err2, `Failed to log plain object fields: ${err2?.errorMessage}`);

    // Test fields with various types (use INFO level to ensure visibility)
    const err3 = tenEnv.logInfo("Test fields with various types", "test", {
      nullValue: null,
      emptyString: "",
      zero: 0,
      falseValue: false,
      emptyArray: [],
      emptyObject: {},
    });
    assert(!err3, `Failed to log various types fields: ${err3?.errorMessage}`);
  }

  async onInit(tenEnv: TenEnv): Promise<void> {
    const err1 = tenEnv.logInfo("DefaultExtension onInit");
    assert(!err1, `Failed to log onInit: ${err1?.errorMessage}`);

    // Test fields with nested structures
    const err2 = tenEnv.logInfo("Test fields with nested structures", "test", {
      user: {
        id: 789,
        name: "test user",
        tags: ["admin", "developer"],
        settings: {
          theme: "dark",
          notifications: true,
        },
      },
      timestamp: Date.now(),
      events: [
        { type: "init", status: "success" },
        { type: "load", status: "pending" },
      ],
    });
    assert(!err2, `Failed to log nested structures fields: ${err2?.errorMessage}`);

    // Test fields with array of different types (use INFO level to ensure visibility)
    const err3 = tenEnv.logInfo("Test fields with mixed array", "test", {
      mixedArray: [1, "string", true, null, { nested: "object" }],
      numbers: [1, 2, 3, 4, 5],
      strings: ["a", "b", "c"],
    });
    assert(!err3, `Failed to log mixed array fields: ${err3?.errorMessage}`);
  }

  async onStart(tenEnv: TenEnv): Promise<void> {
    const err = tenEnv.logInfo("DefaultExtension onStart");
    assert(!err, `Failed to log onStart: ${err?.errorMessage}`);

    const greetingCmd = Cmd.Create("greeting");

    const [greetingMsg] = await tenEnv.getPropertyString("greetingMsg");

    if (greetingMsg) {
      greetingCmd.setPropertyString("greetingMsg", greetingMsg);
    }

    tenEnv.sendCmd(greetingCmd);
  }

  async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
    const cmdName = cmd.getName();

    const err1 = tenEnv.logInfo("DefaultExtension onCmd" + cmdName, "cmd", {
      command: cmdName,
      timestamp: new Date().toISOString(),
    });
    assert(!err1, `Failed to log onCmd: ${err1?.errorMessage}`);

    if (cmdName === "ping") {
      // Test fields with command response information
      const err2 = tenEnv.logInfo("Ping command received", "cmd", {
        status: "ok",
        response: "pong",
        latency: 42,
      });
      assert(!err2, `Failed to log ping command: ${err2?.errorMessage}`);

      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      tenEnv.returnResult(cmdResult);

      const pongCmd = Cmd.Create("pong");
      tenEnv.sendCmd(pongCmd);
    } else {
      // Test fields with error information
      const err3 = tenEnv.logError("Unknown command", "cmd", {
        command: cmdName,
        error: "unknown command",
        availableCommands: ["ping", "pong"],
      });
      assert(!err3, `Failed to log error: ${err3?.errorMessage}`);

      const cmdResult = CmdResult.Create(StatusCode.ERROR, cmd);
      cmdResult.setPropertyString("detail", "unknown command");
      tenEnv.returnResult(cmdResult);
    }
  }

  async onData(tenEnv: TenEnv, data: Data): Promise<void> {
    const err1 = tenEnv.logInfo("DefaultExtension onData", "data", {
      event: "onData",
      dataName: data.getName(),
    });
    assert(!err1, `Failed to log onData: ${err1?.errorMessage}`);

    const dataName = data.getName();
    if (dataName === "ping") {
      // Test fields with complex nested structure
      const err2 = tenEnv.logInfo("Ping data received", "data", {
        dataName: "ping",
        response: {
          type: "pong",
          timestamp: Date.now(),
          metadata: {
            source: "extension",
            processed: true,
          },
        },
        tags: ["ping", "pong", "data"],
      });
      assert(!err2, `Failed to log ping data: ${err2?.errorMessage}`);

      const pongData = Data.Create("pong");
      tenEnv.sendData(pongData);
    } else {
      const err3 = tenEnv.log(LogLevel.ERROR, "unknown data received: " + dataName, "data", {
        dataName: dataName,
        error: "unknown data type",
      });
      assert(!err3, `Failed to log error: ${err3?.errorMessage}`);
    }
  }

  async onVideoFrame(tenEnv: TenEnv, videoFrame: VideoFrame): Promise<void> {
    const err1 = tenEnv.logInfo("DefaultExtension onVideoFrame", "video", {
      frameName: videoFrame.getName(),
      event: "onVideoFrame",
    });
    assert(!err1, `Failed to log onVideoFrame: ${err1?.errorMessage}`);

    const videoFrameName = videoFrame.getName();
    if (videoFrameName === "ping") {
      // Test fields with video frame info
      const err2 = tenEnv.logInfo("Ping video frame received", "video", {
        frameType: "ping",
        width: 1920,
        height: 1080,
        fps: 30,
        codec: "h264",
      });
      assert(!err2, `Failed to log ping video frame: ${err2?.errorMessage}`);

      const pongVideoFrame = VideoFrame.Create("pong");
      tenEnv.sendVideoFrame(pongVideoFrame);
    } else {
      const err3 = tenEnv.log(
        LogLevel.ERROR,
        "unknown video frame received: " + videoFrameName,
        "video",
        {
          frameName: videoFrameName,
          error: "unknown frame type",
        },
      );
      assert(!err3, `Failed to log error: ${err3?.errorMessage}`);
    }
  }

  async onAudioFrame(tenEnv: TenEnv, audioFrame: AudioFrame): Promise<void> {
    const err1 = tenEnv.logInfo("DefaultExtension onAudioFrame", "audio", {
      frameName: audioFrame.getName(),
      event: "onAudioFrame",
    });
    assert(!err1, `Failed to log onAudioFrame: ${err1?.errorMessage}`);

    const audioFrameName = audioFrame.getName();
    if (audioFrameName === "ping") {
      // Test fields with audio frame information
      const err2 = tenEnv.logInfo("Ping audio frame received", "audio", {
        frameType: "ping",
        sampleRate: 44100,
        channels: 2,
        format: "pcm",
        duration: 0.1,
        metadata: {
          bitDepth: 16,
          endianness: "little",
        },
      });
      assert(!err2, `Failed to log ping audio frame: ${err2?.errorMessage}`);

      const pongAudioFrame = AudioFrame.Create("pong");
      tenEnv.sendAudioFrame(pongAudioFrame);
    } else {
      const err3 = tenEnv.log(
        LogLevel.ERROR,
        "unknown audio frame received: " + audioFrameName,
        "audio",
        {
          frameName: audioFrameName,
          error: "unknown frame type",
        },
      );
      assert(!err3, `Failed to log error: ${err3?.errorMessage}`);
    }
  }

  async onStop(tenEnv: TenEnv): Promise<void> {
    // Test fields with minimal object
    const err1 = tenEnv.logInfo("DefaultExtension onStop", "lifecycle", {
      phase: "stop",
      timestamp: Date.now(),
    });
    assert(!err1, `Failed to log onStop: ${err1?.errorMessage}`);

    // Test fields with undefined (should be handled gracefully)
    const err2 = tenEnv.logInfo("Stop log with undefined fields", "lifecycle", undefined);
    assert(!err2, `Failed to log undefined fields: ${err2?.errorMessage}`);
  }

  async onDeinit(tenEnv: TenEnv): Promise<void> {
    // Test fields with array of objects
    const err = tenEnv.logInfo("DefaultExtension onDeinit", "lifecycle", {
      phase: "deinit",
      cleanup: [
        { resource: "connections", status: "closed" },
        { resource: "cache", status: "cleared" },
        { resource: "handlers", status: "unregistered" },
      ],
      finalState: {
        initialized: false,
        running: false,
      },
    });
    assert(!err, `Failed to log onDeinit: ${err?.errorMessage}`);
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
