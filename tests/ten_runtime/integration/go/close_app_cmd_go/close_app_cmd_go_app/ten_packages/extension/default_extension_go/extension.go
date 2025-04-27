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

type defaultExtension struct {
	ten.DefaultExtension
}

func (ext *defaultExtension) OnConfigure(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("OnConfigure")
	tenEnv.OnConfigureDone()
}

func (ext *defaultExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	if cmdName == "close" {
		// Testing get/set property to/from json with empty path.
		testCmd, _ := ten.NewCmd("test")
		testCmd.SetPropertyFromJSONBytes("", []byte("{\"key\":\"value\"}"))

		testValue, _ := testCmd.GetPropertyToJSONBytes("")
		tenEnv.LogDebug("testValue: " + string(testValue))
		// Testing end.

		closeAppCmd, _ := ten.NewCmd("ten:close_app")

		err := closeAppCmd.SetDest("localhost", "", "", "")
		if err != nil {
			tenEnv.LogError("Failed to SetDest:" + err.Error())
			return
		}

		err = tenEnv.SendCmd(closeAppCmd, nil)
		if err != nil {
			tenEnv.LogError("Failed to send close cmd:" + err.Error())
			return
		}

		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult.SetPropertyString("detail", "ok")

		tenEnv.ReturnResult(cmdResult, nil)
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
