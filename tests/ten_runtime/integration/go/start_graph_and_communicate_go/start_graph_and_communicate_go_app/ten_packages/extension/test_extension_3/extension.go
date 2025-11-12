//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package test_extension_3

import (
	ten "ten_framework/ten_runtime"
)

type testExtension3 struct {
	ten.DefaultExtension
}

func (ext *testExtension3) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	if cmdName == "B" {
		ext.handleBCmd(tenEnv, cmd)
	} else {
		tenEnv.LogError("test_extension_3 received unexpected cmd: " + cmdName)
	}
}

func (ext *testExtension3) handleBCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	// Simply return OK status for command B
	result, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	tenEnv.ReturnResult(result, nil)
}

func newTestExtension3(name string) ten.Extension {
	return &testExtension3{}
}

func init() {
	err := ten.RegisterAddonAsExtension(
		"test_extension_3",
		ten.NewDefaultExtensionAddon(newTestExtension3),
	)
	if err != nil {
		panic("Failed to register addon: " + err.Error())
	}
}
