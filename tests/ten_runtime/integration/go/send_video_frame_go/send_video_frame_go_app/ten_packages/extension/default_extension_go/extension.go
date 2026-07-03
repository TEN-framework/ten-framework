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

	videoFrame, error := ten.NewVideoFrame("video_frame")
	if error != nil {
		panic("Failed to create video frame.")
	}

	videoFrame.SetWidth(320)
	videoFrame.SetHeight(240)
	videoFrame.SetPixelFmt(ten.PixelFmtRGBA)
	videoFrame.SetEOF(false)

	now := time.Now()
	timestamp := now.UnixMicro()
	videoFrame.SetTimestamp(timestamp)

	if error := videoFrame.AllocBuf(320 * 240 * 4); error != nil {
		panic("Failed to alloc buf.")
	}

	buf, error := videoFrame.LockBuf()
	if error != nil {
		panic("Failed to lock buf.")
	}

	for i := 0; i < 320*240*4; i++ {
		buf[i] = byte(i % 256)
	}

	if error := videoFrame.UnlockBuf(&buf); error != nil {
		panic("Failed to unlock buf.")
	}

	if error := tenEnv.SendVideoFrame(videoFrame, nil); error != nil {
		panic("Failed to send video frame.")
	}
}

func (ext *defaultExtension) OnVideoFrame(
	tenEnv ten.TenEnv,
	frame ten.VideoFrame,
) {
	frameName, error := frame.GetName()
	if error != nil {
		panic("Failed to get videoFrame name.")
	}

	tenEnv.LogDebug("OnAudioFrame" + frameName)

	pixelFmt, error := frame.GetPixelFmt()
	if error != nil || pixelFmt != ten.PixelFmtRGBA {
		panic("Failed to get pixel fmt.")
	}

	width, error := frame.GetWidth()
	if error != nil || width != 320 {
		panic("Failed to get width.")
	}

	height, error := frame.GetHeight()
	if error != nil || height != 240 {
		panic("Failed to get height.")
	}

	timestamp, error := frame.GetTimestamp()
	if error != nil || timestamp > time.Now().UnixMicro() || timestamp <= 0 {
		panic("Failed to get timestamp.")
	}

	isEOF, error := frame.IsEOF()
	if error != nil || isEOF {
		panic("Failed to get is EOF.")
	}

	buf, err := frame.GetBuf()
	if err != nil {
		panic("Failed to get buf.")
	}

	for i := 0; i < 320*240*4; i++ {
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

	for i := 0; i < 320*240*4; i++ {
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
