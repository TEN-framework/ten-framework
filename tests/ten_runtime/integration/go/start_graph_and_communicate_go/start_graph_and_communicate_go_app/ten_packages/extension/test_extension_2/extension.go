//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package test_extension_2

import (
	ten "ten_framework/ten_runtime"
)

type testExtension2 struct {
	ten.DefaultExtension
}

func (ext *testExtension2) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	if cmdName == "start" {
		ext.handleStartCmd(tenEnv, cmd)
	} else {
		tenEnv.LogError("test_extension_2 received unexpected cmd: " + cmdName)
	}
}

func (ext *testExtension2) handleStartCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdA, _ := ten.NewCmd("A")

	tenEnv.SendCmd(
		cmdA,
		func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
			// Return result for the start command
			result, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			tenEnv.ReturnResult(result, nil)
		},
	)
}

func newTestExtension2(name string) ten.Extension {
	return &testExtension2{}
}

func init() {
	err := ten.RegisterAddonAsExtension(
		"test_extension_2",
		ten.NewDefaultExtensionAddon(newTestExtension2),
	)
	if err != nil {
		panic("Failed to register addon: " + err.Error())
	}
}
