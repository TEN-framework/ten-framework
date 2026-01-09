//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import assert from "assert";
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
    // Test fields with plain object
    const err1 = tenEnvTester.log(LogLevel.INFO, "CmdTester onStart", "test", {
      tester: "CmdTester",
      phase: "start",
      timestamp: Date.now(),
      config: {
        enabled: true,
        timeout: 5000,
      },
    });
    assert(!err1, `Failed to log CmdTester onStart: ${err1?.errorMessage}`);

    // Test fields with command information
    const err2 = tenEnvTester.logInfo("Sending ping command", "test", {
      command: "ping",
      testId: "cmd-test-001",
      metadata: { type: "command", priority: "high" },
    });
    assert(!err2, `Failed to log sending ping command: ${err2?.errorMessage}`);

    const pingCmd = Cmd.Create("ping");
    tenEnvTester.sendCmd(pingCmd);
  }

  async onCmd(tenEnvTester: TenEnvTester, cmd: Cmd) {
    const cmdName = cmd.getName();

    // Test fields with command information
    const err1 = tenEnvTester.logInfo("CmdTester onCmd: " + cmdName, "cmd", {
      command: cmdName,
      tester: "CmdTester",
      received: true,
      arrayField: [1, 2, 3],
      nested: {
        level1: {
          level2: "deep nested value",
        },
      },
    });
    assert(!err1, `Failed to log CmdTester onCmd: ${err1?.errorMessage}`);

    if (cmdName === "pong") {
      // Test fields with response information
      const err2 = tenEnvTester.log(LogLevel.INFO, "pong cmd received", "cmd", {
        command: "pong",
        status: "received",
        response: {
          type: "pong",
          latency: 10,
          success: true,
        },
        tags: ["pong", "response", "success"],
      });
      assert(!err2, `Failed to log pong cmd received: ${err2?.errorMessage}`);

      const cmdResult = CmdResult.Create(StatusCode.OK, cmd);
      await tenEnvTester.returnResult(cmdResult);

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    // Test fields with various types including null and undefined
    tenEnvTester.log(LogLevel.INFO, "CmdTester onStop", "lifecycle", {
      phase: "stop",
      nullValue: null,
      emptyString: "",
      zero: 0,
      falseValue: false,
      emptyArray: [],
      emptyObject: {},
    });
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    // Test fields with undefined (should be handled gracefully)
    const err1 = tenEnvTester.log(LogLevel.INFO, "CmdTester onDeinit", "lifecycle", undefined);
    assert(!err1, `Failed to log CmdTester onDeinit: ${err1?.errorMessage}`);

    // Test fields with cleanup information (use INFO to ensure visibility)
    const err2 = tenEnvTester.logInfo("Cleanup completed", "lifecycle", {
      cleanup: [
        { resource: "cmd-handler", status: "unregistered" },
        { resource: "test-state", status: "cleared" },
      ],
    });
    assert(!err2, `Failed to log cleanup: ${err2?.errorMessage}`);
  }
}

export class DataTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    // Test fields with nested object structure
    tenEnvTester.log(LogLevel.INFO, "DataTester onStart", "test", {
      tester: "DataTester",
      phase: "start",
      testConfig: {
        dataType: "ping",
        expectedResponse: "pong",
        timeout: 3000,
        retries: 3,
      },
      metadata: {
        version: "1.0",
        environment: "test",
      },
    });

    const pingData = Data.Create("ping");
    tenEnvTester.sendData(pingData);
  }

  async onData(tenEnvTester: TenEnvTester, data: Data) {
    const dataName = data.getName();

    // Test fields with data information
    const err1 = tenEnvTester.logInfo("DataTester onData: " + dataName, "data", {
      dataName: dataName,
      tester: "DataTester",
      received: true,
      timestamp: Date.now(),
    });
    assert(!err1, `Failed to log DataTester onData: ${err1?.errorMessage}`);

    if (dataName === "pong") {
      // Test fields with complex nested structure
      const err2 = tenEnvTester.log(LogLevel.INFO, "pong data received", "data", {
        dataName: "pong",
        status: "success",
        response: {
          type: "pong",
          source: "extension",
          processed: true,
          metadata: {
            received: true,
            validated: true,
          },
        },
        arrays: {
          numbers: [1, 2, 3, 4, 5],
          strings: ["a", "b", "c"],
          mixed: [1, "string", true, null],
        },
      });
      assert(!err2, `Failed to log pong data received: ${err2?.errorMessage}`);

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "DataTester onStop", "lifecycle", {
      phase: "stop",
      tester: "DataTester",
    });
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "DataTester onDeinit", "lifecycle", {
      phase: "deinit",
      cleanup: "completed",
    });
  }
}

export class VideoFrameTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    // Test fields with video frame information
    tenEnvTester.log(LogLevel.INFO, "VideoFrameTester onStart", "test", {
      tester: "VideoFrameTester",
      frameType: "ping",
      videoConfig: {
        width: 1920,
        height: 1080,
        fps: 30,
        codec: "h264",
      },
    });

    const pingVideoFrame = VideoFrame.Create("ping");
    tenEnvTester.sendVideoFrame(pingVideoFrame);
  }

  async onVideoFrame(tenEnvTester: TenEnvTester, videoFrame: VideoFrame) {
    const videoFrameName = videoFrame.getName();

    // Test fields with video frame info
    tenEnvTester.logInfo(
      "VideoFrameTester onVideoFrame: " + videoFrameName,
      "video",
      {
        frameName: videoFrameName,
        tester: "VideoFrameTester",
        frameInfo: {
          width: 1920,
          height: 1080,
          timestamp: Date.now(),
        },
      },
    );

    if (videoFrameName === "pong") {
      // Test fields with nested video frame data
      tenEnvTester.log(LogLevel.INFO, "pong video frame received", "video", {
        frameName: "pong",
        status: "received",
        frameData: {
          type: "pong",
          resolution: {
            width: 1920,
            height: 1080,
          },
          metadata: {
            codec: "h264",
            bitrate: 5000,
            fps: 30,
          },
        },
        processing: {
          decoded: true,
          validated: true,
          ready: true,
        },
      });

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "VideoFrameTester onStop", "lifecycle", {
      phase: "stop",
      tester: "VideoFrameTester",
    });
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "VideoFrameTester onDeinit", "lifecycle", {
      phase: "deinit",
      cleanup: ["video-handler", "frame-buffer"],
    });
  }
}

export class AudioFrameTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    // Test fields with audio frame information
    tenEnvTester.log(LogLevel.INFO, "AudioFrameTester onStart", "test", {
      tester: "AudioFrameTester",
      frameType: "ping",
      audioConfig: {
        sampleRate: 44100,
        channels: 2,
        format: "pcm",
        bitDepth: 16,
      },
    });

    const pingAudioFrame = AudioFrame.Create("ping");
    tenEnvTester.sendAudioFrame(pingAudioFrame);
  }

  async onAudioFrame(tenEnvTester: TenEnvTester, audioFrame: AudioFrame) {
    const audioFrameName = audioFrame.getName();

    // Test fields with plain object containing audio info
    tenEnvTester.logInfo(
      "AudioFrameTester onAudioFrame: " + audioFrameName,
      "audio",
      {
        frameName: audioFrameName,
        tester: "AudioFrameTester",
        audioInfo: {
          sampleRate: 44100,
          channels: 2,
          duration: 0.1,
        },
      },
    );

    if (audioFrameName === "pong") {
      // Test fields with complex structure
      tenEnvTester.log(LogLevel.INFO, "pong audio frame received", "audio", {
        frameName: "pong",
        status: "received",
        frameData: {
          type: "pong",
          audio: {
            sampleRate: 44100,
            channels: 2,
            format: "pcm",
          },
          metadata: {
            bitDepth: 16,
            endianness: "little",
            duration: 0.1,
          },
        },
        processing: {
          decoded: true,
          validated: true,
        },
      });

      tenEnvTester.stopTest();
    }
  }

  async onStop(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "AudioFrameTester onStop", "lifecycle", {
      phase: "stop",
      tester: "AudioFrameTester",
    });
  }

  async onDeinit(tenEnvTester: TenEnvTester) {
    tenEnvTester.log(LogLevel.INFO, "AudioFrameTester onDeinit", "lifecycle", {
      phase: "deinit",
      cleanup: {
        audioHandler: "unregistered",
        frameBuffer: "cleared",
      },
    });
  }
}

export class TimeoutTester extends ExtensionTester {
  async onStart(tenEnvTester: TenEnvTester) {
    // Test fields with timeout information
    const err1 = tenEnvTester.log(LogLevel.INFO, "TimeoutTester onStart", "test", {
      tester: "TimeoutTester",
      timeout: 5000,
      expectedBehavior: "timeout",
      testType: "negative",
    });
    assert(!err1, `Failed to log TimeoutTester onStart: ${err1?.errorMessage}`);

    // Test all log levels with fields (use INFO for DEBUG/WARN to ensure visibility)
    const err2 = tenEnvTester.logInfo("Debug log with fields (as INFO)", "test", {
      level: "debug",
      debugInfo: { enabled: true },
    });
    assert(!err2, `Failed to log debug fields: ${err2?.errorMessage}`);

    const err3 = tenEnvTester.logInfo("Info log with fields", "test", {
      level: "info",
      info: "test information",
    });
    assert(!err3, `Failed to log info fields: ${err3?.errorMessage}`);

    const err4 = tenEnvTester.logInfo("Warn log with fields (as INFO)", "test", {
      level: "warn",
      warning: "test warning",
    });
    assert(!err4, `Failed to log warn fields: ${err4?.errorMessage}`);

    const err5 = tenEnvTester.logError("Error log with fields", "test", {
      level: "error",
      error: "test error",
    });
    assert(!err5, `Failed to log error fields: ${err5?.errorMessage}`);

    // Test fields with all log levels using log() method
    const err6 = tenEnvTester.log(LogLevel.INFO, "Debug via log() (as INFO)", "test", {
      method: "log",
      level: LogLevel.DEBUG,
    });
    assert(!err6, `Failed to log via log() DEBUG: ${err6?.errorMessage}`);

    const err7 = tenEnvTester.log(LogLevel.INFO, "Info via log()", "test", {
      method: "log",
      level: LogLevel.INFO,
    });
    assert(!err7, `Failed to log via log() INFO: ${err7?.errorMessage}`);

    const err8 = tenEnvTester.log(LogLevel.INFO, "Warn via log() (as INFO)", "test", {
      method: "log",
      level: LogLevel.WARN,
    });
    assert(!err8, `Failed to log via log() WARN: ${err8?.errorMessage}`);

    const err9 = tenEnvTester.log(LogLevel.ERROR, "Error via log()", "test", {
      method: "log",
      level: LogLevel.ERROR,
    });
    assert(!err9, `Failed to log via log() ERROR: ${err9?.errorMessage}`);
  }
}
