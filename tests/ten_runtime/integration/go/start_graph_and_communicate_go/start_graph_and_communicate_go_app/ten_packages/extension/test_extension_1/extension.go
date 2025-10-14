//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package test_extension_1

import (
	"encoding/json"

	ten "ten_framework/ten_runtime"
)

type testExtension1 struct {
	ten.DefaultExtension

	receivedDataFromNewGraph bool
	newGraphID               string
	testCmd                  ten.Cmd
}

func (ext *testExtension1) OnStart(tenEnv ten.TenEnv) {
	// Start a new graph
	startGraphCmd, _ := ten.NewStartGraphCmd()

	// The destination of the 'start_graph' command is the current app,
	// using "" to represent current app.
	startGraphCmd.SetDests(ten.Loc{
		AppURI:        ten.Ptr(""),
		GraphID:       nil,
		ExtensionName: nil,
	})

	// The new graph contains 3 extensions.
	graphJSON := `{
		"nodes": [{
			"type": "extension",
			"name": "test_extension_2",
			"addon": "test_extension_2"
		}, {
			"type": "extension",
			"name": "test_extension_3",
			"addon": "test_extension_3"
		}, {
			"type": "extension",
			"name": "test_extension_4",
			"addon": "test_extension_4"
		}],
		"connections": [{
			"extension": "test_extension_2",
			"cmd": [{
				"name": "A",
				"dest": [{
					"extension": "test_extension_3",
					"msg_conversion": {
						"keep_original": true,
						"type": "per_property",
						"rules": [{
							"path": "ten.name",
							"conversion_mode": "fixed_value",
							"value": "B"
						}]
					}
				}, {
					"extension": "test_extension_4"
				}]
			}]
		}],
		"exposed_messages": [{
			"type": "cmd_in",
			"name": "start",
			"extension": "test_extension_2"
		}, {
			"type": "data_out",
			"name": "data_from_new_graph",
			"extension": "test_extension_4"
		}]
	}`

	err := startGraphCmd.SetGraphFromJSONBytes([]byte(graphJSON))
	if err != nil {
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
				panic("Start graph command failed")
			}

			// Get the graph ID of the newly created graph
			newGraphID, _ := cmdResult.GetPropertyString("graph_id")
			ext.newGraphID = newGraphID

			tenEnv.LogInfo("new_graph_id: " + newGraphID)

			// Send a 'start' command to test_extension_2
			cmdStart, _ := ten.NewCmd("start")
			cmdStart.SetDests(ten.Loc{
				AppURI:        ten.Ptr(""),
				GraphID:       ten.Ptr(newGraphID),
				ExtensionName: nil,
			})

			tenEnv.SendCmd(
				cmdStart,
				func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
					if err != nil {
						tenEnv.LogError("Start command failed: " + err.Error())
						return
					}

					if cmdResult == nil {
						tenEnv.LogError("Start command result is None")
						return
					}
				},
			)
		},
	)

	tenEnv.OnStartDone()
}

func (ext *testExtension1) OnStop(tenEnv ten.TenEnv) {
	tenEnv.LogInfo("on_stop")

	// Stop the started graph
	stopGraphCmd, _ := ten.NewStopGraphCmd()
	stopGraphCmd.SetDests(ten.Loc{
		AppURI:        ten.Ptr(""),
		GraphID:       nil,
		ExtensionName: nil,
	})
	stopGraphCmd.SetGraphID(ext.newGraphID)

	tenEnv.SendCmd(
		stopGraphCmd,
		func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
			if err != nil {
				tenEnv.LogError("Stop graph failed: " + err.Error())
			}
			tenEnv.OnStopDone()
		},
	)
}

func (ext *testExtension1) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	if cmdName == "test" {
		ext.testCmd = cmd

		if ext.receivedDataFromNewGraph {
			// Send the response to the client.
			ext.replyToClient(tenEnv)
		}
	} else {
		tenEnv.LogError("Should not happen - unknown command: " + cmdName)
	}
}

func (ext *testExtension1) OnData(tenEnv ten.TenEnv, data ten.Data) {
	dataName, _ := data.GetName()

	if dataName == "data_from_new_graph" {
		ext.receivedDataFromNewGraph = true

		if ext.testCmd != nil {
			ext.replyToClient(tenEnv)
		}
	} else {
		tenEnv.LogError("Should not happen - unknown data: " + dataName)
	}
}

func (ext *testExtension1) replyToClient(tenEnv ten.TenEnv) {
	cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, ext.testCmd)

	detail := map[string]interface{}{
		"id":   1,
		"name": "a",
	}

	detailBytes, _ := json.Marshal(detail)
	cmdResult.SetPropertyString("detail", string(detailBytes))

	tenEnv.ReturnResult(cmdResult, nil)
}

func newTestExtension1(name string) ten.Extension {
	return &testExtension1{}
}

func init() {
	err := ten.RegisterAddonAsExtension(
		"test_extension_1",
		ten.NewDefaultExtensionAddon(newTestExtension1),
	)
	if err != nil {
		panic("Failed to register addon: " + err.Error())
	}
}
