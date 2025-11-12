//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package tests

import (
	ten "ten_framework/ten_runtime"
)

// CmdTester is a tester for the Cmd extension.
type CmdTester struct {
	ten.DefaultExtensionTester
}

// OnStart is called when the test starts.
func (tester *CmdTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	pingCmd, _ := ten.NewCmd("ping")
	tenEnvTester.SendCmd(pingCmd, nil)

	tenEnvTester.OnStartDone()
}

// OnCmd is called when a cmd is received.
func (tester *CmdTester) OnCmd(
	tenEnv ten.TenEnvTester,
	cmd ten.Cmd,
) {
	tenEnv.LogInfo("OnCmd")

	cmdName, _ := cmd.GetName()
	if cmdName == "pong" {
		tenEnv.LogInfo("pong cmd received")

		err := tenEnv.StopTest(nil)
		if err != nil {
			panic(err)
		}
	}
}

// DataTester is a tester for the Data extension.
type DataTester struct {
	ten.DefaultExtensionTester
}

// OnStart is called when the test starts.
func (tester *DataTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	// Send ping data
	pingData, _ := ten.NewData("ping")
	tenEnvTester.SendData(pingData, nil)

	tenEnvTester.OnStartDone()
}

// OnData is called when a data is received.
func (tester *DataTester) OnData(
	tenEnv ten.TenEnvTester,
	data ten.Data,
) {
	tenEnv.LogInfo("OnData")

	dataName, _ := data.GetName()
	if dataName == "pong" {
		tenEnv.LogInfo("pong data received")

		err := tenEnv.StopTest(nil)
		if err != nil {
			panic(err)
		}
	} else {
		panic("unknown data received: " + dataName)
	}
}

// VideoFrameTester is a tester for the VideoFrame extension.
type VideoFrameTester struct {
	ten.DefaultExtensionTester
}

// OnStart is called when the test starts.
func (tester *VideoFrameTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	pingVideoFrame, _ := ten.NewVideoFrame("ping")
	tenEnvTester.SendVideoFrame(pingVideoFrame, nil)

	tenEnvTester.OnStartDone()
}

// OnVideoFrame is called when a video frame is received.
func (tester *VideoFrameTester) OnVideoFrame(
	tenEnv ten.TenEnvTester,
	videoFrame ten.VideoFrame,
) {
	tenEnv.LogInfo("OnVideoFrame")

	videoFrameName, _ := videoFrame.GetName()
	if videoFrameName == "pong" {
		tenEnv.LogInfo("pong video frame received")

		err := tenEnv.StopTest(nil)
		if err != nil {
			panic(err)
		}
	}
}

// AudioFrameTester is a tester for the AudioFrame extension.
type AudioFrameTester struct {
	ten.DefaultExtensionTester
}

// OnStart is called when the test starts.
func (tester *AudioFrameTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	pingAudioFrame, _ := ten.NewAudioFrame("ping")
	tenEnvTester.SendAudioFrame(pingAudioFrame, nil)

	tenEnvTester.OnStartDone()
}

// OnAudioFrame is called when an audio frame is received.
func (tester *AudioFrameTester) OnAudioFrame(
	tenEnv ten.TenEnvTester,
	audioFrame ten.AudioFrame,
) {
	tenEnv.LogInfo("OnAudioFrame")

	audioFrameName, _ := audioFrame.GetName()
	if audioFrameName == "pong" {
		tenEnv.LogInfo("pong audio frame received")

		err := tenEnv.StopTest(nil)
		if err != nil {
			panic(err)
		}
	}
}
