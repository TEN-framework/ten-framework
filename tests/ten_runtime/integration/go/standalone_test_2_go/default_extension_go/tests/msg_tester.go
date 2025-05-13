package tests

import (
	ten "ten_framework/ten_runtime"
)

// CmdTester

type CmdTester struct {
	ten.DefaultExtensionTester
}

func (tester *CmdTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	pingCmd, _ := ten.NewCmd("ping")
	tenEnvTester.SendCmd(pingCmd, nil)

	tenEnvTester.OnStartDone()
}

func (tester *CmdTester) OnCmd(
	tenEnv ten.TenEnvTester,
	cmd ten.Cmd,
) {
	tenEnv.LogInfo("OnCmd")

	cmdName, _ := cmd.GetName()
	if cmdName == "pong" {
		tenEnv.LogInfo("pong cmd received")

		tenEnv.StopTest()
	}
}

// DataTester

type DataTester struct {
	ten.DefaultExtensionTester
}

func (tester *DataTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	// Send ping data
	pingData, _ := ten.NewData("ping")
	tenEnvTester.SendData(pingData, nil)

	tenEnvTester.OnStartDone()
}

func (tester *DataTester) OnData(
	tenEnv ten.TenEnvTester,
	data ten.Data,
) {
	tenEnv.LogInfo("OnData")

	dataName, _ := data.GetName()
	if dataName == "pong" {
		tenEnv.LogInfo("pong data received")

		tenEnv.StopTest()
	} else {
		panic("unknown data received: " + dataName)
	}
}

// VideoFrameTester

type VideoFrameTester struct {
	ten.DefaultExtensionTester
}

func (tester *VideoFrameTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	pingVideoFrame, _ := ten.NewVideoFrame("ping")
	tenEnvTester.SendVideoFrame(pingVideoFrame, nil)

	tenEnvTester.OnStartDone()
}

func (tester *VideoFrameTester) OnVideoFrame(
	tenEnv ten.TenEnvTester,
	videoFrame ten.VideoFrame,
) {
	tenEnv.LogInfo("OnVideoFrame")

	videoFrameName, _ := videoFrame.GetName()
	if videoFrameName == "pong" {
		tenEnv.LogInfo("pong video frame received")

		tenEnv.StopTest()
	}
}

// AudioFrameTester

type AudioFrameTester struct {
	ten.DefaultExtensionTester
}

func (tester *AudioFrameTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	pingAudioFrame, _ := ten.NewAudioFrame("ping")
	tenEnvTester.SendAudioFrame(pingAudioFrame, nil)

	tenEnvTester.OnStartDone()
}

func (tester *AudioFrameTester) OnAudioFrame(
	tenEnv ten.TenEnvTester,
	audioFrame ten.AudioFrame,
) {
	tenEnv.LogInfo("OnAudioFrame")

	audioFrameName, _ := audioFrame.GetName()
	if audioFrameName == "pong" {
		tenEnv.LogInfo("pong audio frame received")

		tenEnv.StopTest()
	}
}
