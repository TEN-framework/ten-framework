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

type aExtension struct {
	name      string
	isStopped bool
	ten.DefaultExtension
}

func newAExtension(name string) ten.Extension {
	return &aExtension{name: name, isStopped: false}
}

func (p *aExtension) OnDeinit(tenEnv ten.TenEnv) {
	defer tenEnv.OnDeinitDone()

	tenEnv.LogDebug("onDeinit")
	if !p.isStopped {
		panic("should not happen.")
	}
}

func (p *aExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		cmdName, _ := cmd.GetName()
		tenEnv.LogInfo(
			"receive command: " + cmdName,
		)
		if cmdName == "start" {
			tenEnv.SendCmd(cmd, func(r ten.TenEnv, cs ten.CmdResult, e error) {
				r.ReturnResult(cs, nil)
			})
		}
	}()
}

func (p *aExtension) OnStop(tenEnv ten.TenEnv) {
	go func() {
		tenEnv.LogDebug("onStop ")

		cmd, _ := ten.NewCmd("stop")
		respChan := make(chan ten.CmdResult, 1)

		tenEnv.SendCmd(
			cmd,
			func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, e error) {
				respChan <- cmdResult
			},
		)

		select {
		case resp := <-respChan:
			statusCode, _ := resp.GetStatusCode()
			if statusCode == ten.StatusCodeOk {
				p.isStopped = true
				tenEnv.OnStopDone()
			} else {
				panic("stop failed.")
			}
		}
	}()
}

func init() {
	fmt.Println("call init")

	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_a",
		ten.NewDefaultExtensionAddon(newAExtension),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
