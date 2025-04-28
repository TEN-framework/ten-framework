// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to https://github.com/TEN-framework/ten_framework/LICENSE for more
// information.
//
// Note that this is just an example extension written in the GO programming
// language, so the package name does not equal to the containing directory
// name. However, it is not common in Go.
package default_extension_go

import (
	"fmt"

	ten "ten_framework/ten_runtime"
)

type bExtension struct {
	ten.DefaultExtension
	stopChan chan struct{}
}

func NewBExtension(name string) ten.Extension {
	return &bExtension{
		stopChan: make(chan struct{}),
	}
}

func (p *bExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		cmdName, _ := cmd.GetName()
		tenEnv.LogInfo(
			"receive command: " +
				cmdName,
		)
		if cmdName == "start" {
			tenEnv.SendCmd(cmd, func(r ten.TenEnv, cs ten.CmdResult, e error) {
				r.ReturnResult(cs, nil)
			})
		} else if cmdName == "stop" {
			tenEnv.SendCmd(cmd, func(r ten.TenEnv, cs ten.CmdResult, e error) {
				r.ReturnResult(cs, nil)

				close(p.stopChan)
				tenEnv.LogInfo("Stop command is processed.")
			})
		} else {
			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
			cmdResult.SetPropertyString("detail", "unknown cmd")
			tenEnv.ReturnResult(cmdResult, nil)
		}
	}()
}

func (p *bExtension) OnStop(tenEnv ten.TenEnv) {
	go func() {
		tenEnv.LogDebug("OnStop")

		// Wait until the stop command is received and processed.
		<-p.stopChan

		tenEnv.LogInfo("Stop command processed. Now calling OnStopDone.")
		tenEnv.OnStopDone()
	}()
}

func init() {
	fmt.Println("call init")

	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_b",
		ten.NewDefaultExtensionAddon(NewBExtension),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
