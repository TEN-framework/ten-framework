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

	if cmdName == "set_original_graph_info" {
		ext.handleSetOriginalGraphInfoCmd(tenEnv, cmd)
	} else if cmdName == "start" {
		ext.handleStartCmd(tenEnv, cmd)
	} else {
		panic("test_extension_2 received unexpected cmd: " + cmdName)
	}
}

func (ext *testExtension2) handleSetOriginalGraphInfoCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	// Get the original graph receiver extension and graph ID
	originalGraphReceiverExtension, _ := cmd.GetPropertyString(
		"original_graph_receiver_extension",
	)
	srcLoc, _ := cmd.GetSource()
	originalGraphID := *srcLoc.GraphID

	cmdSetOriginalGraphInfo, _ := ten.NewCmd("set_original_graph_info")
	cmdSetOriginalGraphInfo.SetPropertyString(
		"original_graph_receiver_extension",
		originalGraphReceiverExtension,
	)
	cmdSetOriginalGraphInfo.SetPropertyString(
		"original_graph_id",
		originalGraphID,
	)

	tenEnv.SendCmd(
		cmdSetOriginalGraphInfo,
		func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
			// Return result for the original command
			result, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			tenEnv.ReturnResult(result, nil)
		},
	)
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
