//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package test_extension_4

import (
	ten "ten_framework/ten_runtime"
)

type testExtension4 struct {
	ten.DefaultExtension
}

func (ext *testExtension4) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	if cmdName == "A" {
		ext.handleACmd(tenEnv, cmd)
	} else {
		tenEnv.LogError("test_extension_4 received unexpected cmd: " + cmdName)
	}
}

func (ext *testExtension4) handleACmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	// Return OK result for command A
	result, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	tenEnv.ReturnResult(result, nil)

	// Send data back to the original graph
	data, _ := ten.NewData("data_from_new_graph")

	tenEnv.SendData(data, func(tenEnv ten.TenEnv, err error) {
		if err != nil {
			tenEnv.LogError("Failed to send data: " + err.Error())
		}
	})
}

func newTestExtension4(name string) ten.Extension {
	return &testExtension4{}
}

func init() {
	err := ten.RegisterAddonAsExtension(
		"test_extension_4",
		ten.NewDefaultExtensionAddon(newTestExtension4),
	)
	if err != nil {
		panic("Failed to register addon: " + err.Error())
	}
}
