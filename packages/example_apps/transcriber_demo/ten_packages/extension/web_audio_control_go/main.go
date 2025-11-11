//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package web_audio_control

import (
	"fmt"
	"ten_packages/extension/web_audio_control_go/server"

	ten "ten_framework/ten_runtime"
)

var (
	logCategoryKeyPoint = "key_point"
)

type webAudioControlExtension struct {
	ten.DefaultExtension
	server *server.WebServer
}

func newWebAudioControlExtension(name string) ten.Extension {
	return &webAudioControlExtension{}
}

func (e *webAudioControlExtension) OnStart(tenEnv ten.TenEnv) {
	tenEnv.Log(
		ten.LogLevelInfo,
		"WebAudioControlExtension OnStart",
		&logCategoryKeyPoint,
		nil,
		nil,
	)

	// Get HTTP port from property
	port, err := tenEnv.GetPropertyInt64("http_port")
	if err != nil {
		tenEnv.LogWarn(
			fmt.Sprintf(
				"Failed to get http_port property, using default 8080: %v",
				err,
			),
		)
		port = 8080
	}

	// Create and start web server
	e.server = server.NewWebServer(int(port), tenEnv)

	// Set audio data handler
	e.server.SetAudioDataHandler(
		func(audioData []byte, sampleRate int, channels int, samplesPerChannel int) {
			e.handleAudioData(
				tenEnv,
				audioData,
				sampleRate,
				channels,
				samplesPerChannel,
			)
		},
	)

	go e.server.Start()

	tenEnv.Log(
		ten.LogLevelInfo,
		fmt.Sprintf("Web server started on port %d", port),
		&logCategoryKeyPoint,
		nil,
		nil,
	)
	tenEnv.OnStartDone()
}

func (e *webAudioControlExtension) OnStop(tenEnv ten.TenEnv) {
	tenEnv.Log(
		ten.LogLevelInfo,
		"WebAudioControlExtension OnStop",
		&logCategoryKeyPoint,
		nil,
		nil,
	)

	if e.server != nil {
		e.server.Stop()
	}

	tenEnv.OnStopDone()
}

func (e *webAudioControlExtension) OnData(tenEnv ten.TenEnv, data ten.Data) {
	dataName, err := data.GetName()
	if err != nil {
		tenEnv.LogError(fmt.Sprintf("Failed to get data name: %v", err))
		return
	}

	tenEnv.LogDebug(fmt.Sprintf("OnData: %s", dataName))

	if dataName == "asr_result" {
		// Extract text property from data
		text, err := data.GetPropertyString("text")
		if err != nil {
			tenEnv.LogError(fmt.Sprintf("Failed to get text property: %v", err))
			return
		}

		// Extract final property from data
		final, err := data.GetPropertyBool("final")
		if err != nil {
			tenEnv.LogError(
				fmt.Sprintf("Failed to get final property: %v", err),
			)
			return
		}

		// Log final results with key_point category
		if final {
			tenEnv.Log(
				ten.LogLevelInfo,
				fmt.Sprintf("Received final ASR result: text='%s'", text),
				&logCategoryKeyPoint,
				nil,
				nil,
			)
		} else {
			tenEnv.LogInfo(fmt.Sprintf("Received interim ASR result: text='%s'", text))
		}

		// Send ASR result to all connected WebSocket clients
		if e.server != nil {
			e.server.BroadcastAsrResult(text, final)
		}
	}
}

func (e *webAudioControlExtension) handleAudioData(
	tenEnv ten.TenEnv,
	audioData []byte,
	sampleRate int,
	channels int,
	samplesPerChannel int,
) {
	// Create audio frame
	audioFrame, err := ten.NewAudioFrame("pcm_frame")
	if err != nil {
		tenEnv.LogError(fmt.Sprintf("Failed to create audio frame: %v", err))
		return
	}

	// Set audio frame properties
	audioFrame.SetSampleRate(int32(sampleRate))
	audioFrame.SetChannelLayout(0)  // Mono
	audioFrame.SetBytesPerSample(2) // 16-bit PCM
	audioFrame.SetNumberOfChannels(int32(channels))
	audioFrame.SetDataFmt(ten.AudioFrameDataFmtInterleave)
	audioFrame.SetSamplesPerChannel(int32(samplesPerChannel))

	// Allocate and fill buffer
	if err := audioFrame.AllocBuf(len(audioData)); err != nil {
		tenEnv.LogError(
			fmt.Sprintf("Failed to allocate audio frame buffer: %v", err),
		)
		return
	}

	buf, err := audioFrame.LockBuf()
	if err != nil {
		tenEnv.LogError(
			fmt.Sprintf("Failed to lock audio frame buffer: %v", err),
		)
		return
	}
	copy(buf, audioData)
	audioFrame.UnlockBuf(&buf)

	// Send audio frame
	if err := tenEnv.SendAudioFrame(audioFrame, nil); err != nil {
		tenEnv.LogError(fmt.Sprintf("Failed to send audio frame: %v", err))
		return
	}
}

func init() {
	fmt.Println("webAudioControlExtension init")

	// Register addon
	ten.RegisterAddonAsExtension(
		"web_audio_control_go",
		ten.NewDefaultExtensionAddon(newWebAudioControlExtension),
	)
}
