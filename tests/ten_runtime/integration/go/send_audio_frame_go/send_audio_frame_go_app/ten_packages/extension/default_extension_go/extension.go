//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	ten "ten_framework/ten_runtime"
	"time"
)

type defaultExtension struct {
	ten.DefaultExtension

	cachedCmd ten.Cmd
}

func (ext *defaultExtension) OnConfigure(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("OnConfigure")
	tenEnv.OnConfigureDone()
}

func (ext *defaultExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	cmdName, error := cmd.GetName()
	if error != nil {
		panic("Failed to get cmd name.")
	}

	tenEnv.LogDebug("OnCmd" + cmdName)

	ext.cachedCmd = cmd

	audioFrame, error := ten.NewAudioFrame("audio_frame")
	if error != nil {
		panic("Failed to create audio frame.")
	}

	audioFrame.SetDataFmt(ten.AudioFrameDataFmtInterleave)
	audioFrame.SetBytesPerSample(2)
	audioFrame.SetSampleRate(16000)
	audioFrame.SetNumberOfChannels(1)
	audioFrame.SetSamplesPerChannel(160)

	now := time.Now()
	timestamp := now.UnixMicro()
	audioFrame.SetTimestamp(timestamp)

	audioFrame.SetEOF(false)
	audioFrame.SetLineSize(320)

	if error := audioFrame.AllocBuf(320); error != nil {
		panic("Failed to alloc buf.")
	}

	buf, error := audioFrame.LockBuf()
	if error != nil {
		panic("Failed to lock buf.")
	}

	for i := 0; i < 320; i++ {
		buf[i] = byte(i % 256)
	}

	if error := audioFrame.UnlockBuf(&buf); error != nil {
		panic("Failed to unlock buf.")
	}

	if error := tenEnv.SendAudioFrame(audioFrame, nil); error != nil {
		panic("Failed to send audio frame.")
	}
}

func (ext *defaultExtension) OnAudioFrame(
	tenEnv ten.TenEnv,
	frame ten.AudioFrame,
) {
	frameName, error := frame.GetName()
	if error != nil {
		panic("Failed to get audioFrame name.")
	}

	tenEnv.LogDebug("OnAudioFrame" + frameName)

	dataFmt, err := frame.GetDataFmt()
	if err != nil || dataFmt != ten.AudioFrameDataFmtInterleave {
		panic("Failed to get data fmt.")
	}

	bytesPerSample, err := frame.GetBytesPerSample()
	if err != nil || bytesPerSample != 2 {
		panic("Failed to get bytes per sample.")
	}

	sampleRate, err := frame.GetSampleRate()
	if err != nil || sampleRate != 16000 {
		panic("Failed to get sample rate.")
	}

	numberOfChannels, err := frame.GetNumberOfChannels()
	if err != nil || numberOfChannels != 1 {
		panic("Failed to get number of channels.")
	}

	samplesPerChannel, err := frame.GetSamplesPerChannel()
	if err != nil || samplesPerChannel != 160 {
		panic("Failed to get samples per channel.")
	}

	timestamp, err := frame.GetTimestamp()
	if err != nil || timestamp > time.Now().UnixMicro() || timestamp <= 0 {
		panic("Failed to get timestamp.")
	}

	eof, err := frame.IsEOF()
	if err != nil || eof {
		panic("Failed to get eof.")
	}

	lineSize, err := frame.GetLineSize()
	if err != nil || lineSize != 320 {
		panic("Failed to get line size.")
	}

	buf, err := frame.GetBuf()
	if err != nil {
		panic("Failed to get buf.")
	}

	for i := 0; i < 320; i++ {
		if buf[i] != byte(i%256) {
			panic("Failed to compare buf.")
		}

		// Reset the copied buffer which won't influence the original buffer.
		buf[i] = 0
	}

	buf, err = frame.LockBuf()
	if err != nil {
		panic("Failed to lock buf.")
	}

	for i := 0; i < 320; i++ {
		if buf[i] != byte(i%256) {
			panic("Failed to compare buf.")
		}
	}

	if err := frame.UnlockBuf(&buf); err != nil {
		panic("Failed to unlock buf.")
	}

	if ext.cachedCmd == nil {
		panic("Cached cmd is nil.")
	}

	cmdResult, err := ten.NewCmdResult(ten.StatusCodeOk, ext.cachedCmd)
	if err != nil {
		panic("Failed to create cmd result.")
	}

	cmdResult.SetPropertyString("detail", "success")
	if err := tenEnv.ReturnResult(cmdResult, nil); err != nil {
		panic("Failed to return result.")
	}
}

func newAExtension(name string) ten.Extension {
	return &defaultExtension{}
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
