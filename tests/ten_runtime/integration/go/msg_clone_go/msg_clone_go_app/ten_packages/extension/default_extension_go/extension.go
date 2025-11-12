//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	ten "ten_framework/ten_runtime"
)

type serverExtension struct {
	ten.DefaultExtension
}

func (ext *serverExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	name, _ := cmd.GetName()
	if name == "test" {
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult.SetPropertyString("detail", "ok")
		tenEnv.ReturnResult(cmdResult, nil)
	} else {
		panic("unknown cmd name: " + name)
	}
}

func (ext *serverExtension) OnData(tenEnv ten.TenEnv, data ten.Data) {
	name, _ := data.GetName()
	if name == "test" {
		clonedData, _ := data.Clone()
		tenEnv.SendData(clonedData, nil)
	} else {
		panic("unknown data name: " + name)
	}
}

func (ext *serverExtension) OnVideoFrame(
	tenEnv ten.TenEnv,
	videoFrame ten.VideoFrame,
) {
	name, _ := videoFrame.GetName()
	if name == "test" {
		clonedVideoFrame, _ := videoFrame.Clone()
		tenEnv.SendVideoFrame(clonedVideoFrame, nil)
	} else {
		panic("unknown video frame name: " + name)
	}
}

func (ext *serverExtension) OnAudioFrame(
	tenEnv ten.TenEnv,
	audioFrame ten.AudioFrame,
) {
	name, _ := audioFrame.GetName()
	if name == "test" {
		clonedAudioFrame, _ := audioFrame.Clone()
		tenEnv.SendAudioFrame(clonedAudioFrame, nil)
	} else {
		panic("unknown audio frame name: " + name)
	}
}

type clientExtension struct {
	ten.DefaultExtension

	msgCount int
	testCmd  ten.Cmd
}

func (ext *clientExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	if cmdName == "test" {
		ext.testCmd = cmd

		data, _ := ten.NewData("test")
		videoFrame, _ := ten.NewVideoFrame("test")
		audioFrame, _ := ten.NewAudioFrame("test")

		tenEnv.SendData(data, nil)
		tenEnv.SendVideoFrame(videoFrame, nil)
		tenEnv.SendAudioFrame(audioFrame, nil)

		clonedCmd, _ := cmd.Clone()
		tenEnv.SendCmd(
			clonedCmd,
			func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
				if err != nil {
					panic("Failed to send cmd: " + err.Error())
				}

				statusCode, _ := cmdResult.GetStatusCode()
				if statusCode != ten.StatusCodeOk {
					panic("Failed to send cmd")
				}

				ext.msgCount++
				ext.returnIfAllDone(tenEnv)
			},
		)
	}
}

func (ext *clientExtension) returnIfAllDone(tenEnv ten.TenEnv) {
	if ext.msgCount == 4 {
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, ext.testCmd)
		cmdResult.SetPropertyString("detail", "ok")

		tenEnv.ReturnResult(cmdResult, nil)
	}
}

func (ext *clientExtension) OnData(tenEnv ten.TenEnv, data ten.Data) {
	name, _ := data.GetName()
	if name == "test" {
		ext.msgCount++
		ext.returnIfAllDone(tenEnv)
	} else {
		panic("unknown data name: " + name)
	}
}

func (ext *clientExtension) OnVideoFrame(
	tenEnv ten.TenEnv,
	videoFrame ten.VideoFrame,
) {
	name, _ := videoFrame.GetName()
	if name == "test" {
		ext.msgCount++
		ext.returnIfAllDone(tenEnv)
	} else {
		panic("unknown video frame name: " + name)
	}
}

func (ext *clientExtension) OnAudioFrame(
	tenEnv ten.TenEnv,
	audioFrame ten.AudioFrame,
) {
	name, _ := audioFrame.GetName()
	if name == "test" {
		ext.msgCount++
		ext.returnIfAllDone(tenEnv)
	} else {
		panic("unknown audio frame name: " + name)
	}
}

func newAExtension(name string) ten.Extension {
	if name == "server" {
		return &serverExtension{}
	} else if name == "client" {
		return &clientExtension{}
	}

	return nil
}

func init() {
	// Register addon.
	err := ten.RegisterAddonAsExtension(
		"default_extension_go",
		ten.NewDefaultExtensionAddon(newAExtension),
	)
	if err != nil {
		panic("Failed to register addon.")
	}
}
