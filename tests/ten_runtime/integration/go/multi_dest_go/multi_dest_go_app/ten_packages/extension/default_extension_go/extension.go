//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"strconv"
	"strings"
	ten "ten_framework/ten_runtime"
)

type serverExtension struct {
	ten.DefaultExtension

	returnOk bool
}

func (ext *serverExtension) OnInit(tenEnv ten.TenEnv) {
	var err error
	ext.returnOk, err = tenEnv.GetPropertyBool("return_ok")
	if err != nil {
		panic("Failed to get property: " + err.Error())
	}

	tenEnv.OnInitDone()
}

func (ext *serverExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	if ext.returnOk {
		newCmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		tenEnv.ReturnResult(newCmdResult, nil)
	} else {
		newCmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
		tenEnv.ReturnResult(newCmdResult, nil)
	}
}

type clientExtension struct {
	ten.DefaultExtension
}

func (ext *clientExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	if cmdName == "test" {
		receivedOkCount := 0
		receivedErrCount := 0

		newCmd, _ := ten.NewCmd("test")

		tenEnv.SendCmdEx(
			newCmd,
			func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
				if err != nil {
					panic("Failed to send cmd: " + err.Error())
				}

				statusCode, _ := cmdResult.GetStatusCode()
				if statusCode == ten.StatusCodeOk {
					receivedOkCount++
				} else {
					receivedErrCount++
				}

				completed, _ := cmdResult.IsCompleted()
				tenEnv.LogInfo("completed: " + strconv.FormatBool(completed))
				if completed {
					if receivedOkCount != 2 || receivedErrCount != 1 {
						panic("Invalid number of received ok or err" +
							"receivedOkCount: " + strconv.Itoa(receivedOkCount) +
							"receivedErrCount: " + strconv.Itoa(receivedErrCount))
					}

					newCmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
					newCmdResult.SetPropertyString("detail", "ok")
					tenEnv.ReturnResult(newCmdResult, nil)
				}
			},
		)

	}
}

func newAExtension(name string) ten.Extension {
	// if name starts with "server", return serverExtension
	if strings.HasPrefix(name, "server") {
		return &serverExtension{}
	} else if strings.HasPrefix(name, "client") {
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
