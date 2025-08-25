//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"fmt"

	ten "ten_framework/ten_runtime"
)

type defaultExtension struct {
	ten.DefaultExtension
}

func newExtension(name string) ten.Extension {
	return &defaultExtension{}
}

func (e *defaultExtension) OnStart(tenEnv ten.TenEnv) {
	greetingCmd, _ := ten.NewCmd("greeting")

	greetingMsg, _ := tenEnv.GetPropertyString("greetingMsg")
	if greetingMsg != "" {
		greetingCmd.SetPropertyString("greetingMsg", greetingMsg)
	}

	tenEnv.SendCmd(greetingCmd, nil)

	tenEnv.OnStartDone()
}

func (e *defaultExtension) OnStop(tenEnv ten.TenEnv) {
	tenEnv.OnStopDone()
}

func (e *defaultExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	tenEnv.LogInfo(fmt.Sprintf("OnCmd: %s", cmdName))

	if cmdName == "ping" {
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		tenEnv.ReturnResult(cmdResult, nil)

		pongCmd, _ := ten.NewCmd("pong")
		tenEnv.SendCmd(pongCmd, nil)
	} else {
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
		cmdResult.SetPropertyString("detail", "unknown command")
		tenEnv.ReturnResult(cmdResult, nil)
	}
}

func (e *defaultExtension) OnData(
	tenEnv ten.TenEnv,
	data ten.Data,
) {
	tenEnv.LogInfo("OnData")

	dataName, _ := data.GetName()
	if dataName == "ping" {
		tenEnv.LogInfo("ping data received")

		newData, _ := ten.NewData("pong")
		tenEnv.SendData(newData, nil)
	} else {
		tenEnv.LogInfo("unknown data received")
	}
}

func (e *defaultExtension) OnVideoFrame(
	tenEnv ten.TenEnv,
	videoFrame ten.VideoFrame,
) {
	tenEnv.LogInfo("OnVideoFrame")

	videoFrameName, _ := videoFrame.GetName()
	if videoFrameName == "ping" {
		tenEnv.LogInfo("ping video frame received")

		newVideoFrame, _ := ten.NewVideoFrame("pong")
		tenEnv.SendVideoFrame(newVideoFrame, nil)
	} else {
		tenEnv.LogInfo("unknown video frame received")
	}
}

func (e *defaultExtension) OnAudioFrame(
	tenEnv ten.TenEnv,
	audioFrame ten.AudioFrame,
) {
	tenEnv.LogInfo("OnAudioFrame")

	audioFrameName, _ := audioFrame.GetName()
	if audioFrameName == "ping" {
		tenEnv.LogInfo("ping audio frame received")

		newAudioFrame, _ := ten.NewAudioFrame("pong")
		tenEnv.SendAudioFrame(newAudioFrame, nil)
	} else {
		tenEnv.LogInfo("unknown audio frame received")
	}
}

func init() {
	fmt.Println("defaultExtension init")

	// Register addon
	ten.RegisterAddonAsExtension(
		"default_extension_go",
		ten.NewDefaultExtensionAddon(newExtension),
	)
}
