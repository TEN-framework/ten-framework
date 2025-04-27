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
	name string
	ten.DefaultExtension
}

func NewExtensionA(name string) ten.Extension {
	return &aExtension{name: name}
}

func (p *aExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		cmdName, _ := cmd.GetName()
		tenEnv.LogInfo(
			"aExtension receive command: " + cmdName,
		)

		if cmdName == "A" {
			cmdB, _ := ten.NewCmd("B")
			err := cmdB.SetProperty("data", 2)
			if err != nil {
				panic("Should not happen.")
			}
			err = tenEnv.SendCmd(
				cmdB,
				func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, e error) {
					detail, err := cmdResult.GetPropertyString("detail")
					if err != nil {
						panic("Should not happen.")
					}
					statusCode, _ := cmdResult.GetStatusCode()
					tenEnv.LogInfo(
						"statusCode:" + fmt.Sprintf(
							"%d",
							statusCode,
						) + " detail: " + detail,
					)

					cmdResult2, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
					cmdResult2.SetPropertyString("detail", detail)
					err = tenEnv.ReturnResult(cmdResult2, nil)
					if err != nil {
						panic("Should not happen.")
					}
				},
			)
			if err != nil {
				panic("sendCmd failed")
			}
		} else {
		}
	}()
}

func (p *aExtension) OnData(
	tenEnv ten.TenEnv,
	data ten.Data,
) {
	fmt.Println("aExtension onData")
}

func init() {
	fmt.Println("call init")

	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_a",
		ten.NewDefaultExtensionAddon(NewExtensionA),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
