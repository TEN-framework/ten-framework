//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// test_extension_1: orchestrator living in the predefined "default" graph.
//
// Flow:
//   1. On start: launch a dynamic graph with sync_stop_before_deinit=true
//      (contains test_extension_2 and test_extension_3).
//   2. Immediately stop the dynamic graph so both extensions enter on_stop.
//   3. test_extension_2 sleeps 2 s in on_stop, then sends "on_stop_notify".
//   4. We receive "on_stop_notify" and return a result so test_extension_2
//      can proceed to call on_stop_done.
//   5. Once stop_graph is done AND the notification is received, reply to the
//      client with OK.  The client's assertion proves the message was
//      delivered while the graph was still in its stop phase.
//

package test_extension_1

import (
	"encoding/json"

	ten "ten_framework/ten_runtime"
)

type testExtension1 struct {
	ten.DefaultExtension

	newGraphID          string
	testCmd             ten.Cmd
	stopGraphDone       bool
	gotOnStopNotify     bool
}

func (ext *testExtension1) OnStart(tenEnv ten.TenEnv) {
	startGraphCmd, _ := ten.NewStartGraphCmd()
	startGraphCmd.SetDests(ten.Loc{
		AppURI:        ten.Ptr(""),
		GraphID:       nil,
		ExtensionName: nil,
	})

	// Enable sync_stop_before_deinit so every extension in the dynamic graph
	// must call on_stop_done before any of them is allowed to proceed to
	// on_deinit.
	if err := startGraphCmd.SetSyncStopBeforeDeinit(true); err != nil {
		panic("Failed to set sync_stop_before_deinit: " + err.Error())
	}

	// Dynamic graph: extension_2 (slow stopper) and extension_3 (fast stopper).
	// extension_2 exposes on_stop_notify as cmd_out so the message can cross
	// the graph boundary and reach test_extension_1.
	graphJSON := `{
		"nodes": [{
			"type": "extension",
			"name": "test_extension_2",
			"addon": "test_extension_2",
			"extension_group": "group_2"
		}, {
			"type": "extension",
			"name": "test_extension_3",
			"addon": "test_extension_3",
			"extension_group": "group_3"
		}],
		"exposed_messages": [{
			"type": "cmd_out",
			"name": "on_stop_notify",
			"extension": "test_extension_2"
		}]
	}`

	if err := startGraphCmd.SetGraphFromJSONBytes([]byte(graphJSON)); err != nil {
		panic("Failed to set graph JSON: " + err.Error())
	}

	tenEnv.SendCmd(
		startGraphCmd,
		func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
			if err != nil {
				panic("Failed to start graph: " + err.Error())
			}

			statusCode, _ := cmdResult.GetStatusCode()
			if statusCode != ten.StatusCodeOk {
				panic("start_graph command failed")
			}

			newGraphID, _ := cmdResult.GetPropertyString("graph_id")
			ext.newGraphID = newGraphID
			tenEnv.LogInfo("Dynamic graph started, id=" + newGraphID)

			// Immediately stop the dynamic graph to trigger on_stop for both
			// extensions.
			stopGraphCmd, _ := ten.NewStopGraphCmd()
			stopGraphCmd.SetDests(ten.Loc{
				AppURI:        ten.Ptr(""),
				GraphID:       nil,
				ExtensionName: nil,
			})
			stopGraphCmd.SetGraphID(newGraphID)

			tenEnv.SendCmd(
				stopGraphCmd,
				func(tenEnv ten.TenEnv, _ ten.CmdResult, err error) {
					if err != nil {
						tenEnv.LogError("stop_graph failed: " + err.Error())
					}
					tenEnv.LogInfo("stop_graph done")
					ext.stopGraphDone = true
					ext.tryReply(tenEnv)
				},
			)
		},
	)

	tenEnv.OnStartDone()
}

func (ext *testExtension1) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	switch cmdName {
	case "test":
		ext.testCmd = cmd
		ext.tryReply(tenEnv)

	case "on_stop_notify":
		tenEnv.LogInfo("Received on_stop_notify from dynamic graph")
		ext.gotOnStopNotify = true

		// Reply so test_extension_2's send_cmd callback fires and it can call
		// on_stop_done.
		result, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		tenEnv.ReturnResult(result, nil)

		ext.tryReply(tenEnv)

	default:
		tenEnv.LogError("Unexpected cmd: " + cmdName)
	}
}

func (ext *testExtension1) tryReply(tenEnv ten.TenEnv) {
	if ext.testCmd == nil || !ext.stopGraphDone || !ext.gotOnStopNotify {
		return
	}

	tenEnv.LogInfo("All conditions met, replying to client")

	cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, ext.testCmd)
	detail := map[string]interface{}{
		"id":   1,
		"name": "a",
	}
	detailBytes, _ := json.Marshal(detail)
	cmdResult.SetPropertyString("detail", string(detailBytes))

	tenEnv.ReturnResult(cmdResult, nil)
	ext.testCmd = nil
}

func newTestExtension1(name string) ten.Extension {
	return &testExtension1{}
}

func init() {
	if err := ten.RegisterAddonAsExtension(
		"test_extension_1",
		ten.NewDefaultExtensionAddon(newTestExtension1),
	); err != nil {
		panic("Failed to register addon: " + err.Error())
	}
}
