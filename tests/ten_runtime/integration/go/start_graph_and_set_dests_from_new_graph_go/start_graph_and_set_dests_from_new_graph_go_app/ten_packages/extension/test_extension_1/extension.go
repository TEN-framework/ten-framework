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

	startAndStopGraphIsCompleted bool
	receivedDataFromNewGraph     bool
	testCmd                      ten.Cmd
}

func (ext *testExtension1) OnStart(tenEnv ten.TenEnv) {
	// Start a new graph
	startGraphCmd, _ := ten.NewStartGraphCmd()

	// Set the destination to current app (empty string)
	startGraphCmd.SetDests(ten.Loc{
		AppURI:        ten.Ptr(""),
		GraphID:       nil,
		ExtensionName: nil,
	})

	// Define the new graph JSON
	graphJSON := `{
		"nodes": [{
			"type": "extension",
			"name": "test_extension_2",
			"addon": "test_extension_2",
			"extension_group": "test_extension_2_group"
		}, {
			"type": "extension",
			"name": "test_extension_3",
			"addon": "test_extension_3",
			"extension_group": "test_extension_3_group"
		}, {
			"type": "extension",
			"name": "test_extension_4",
			"addon": "test_extension_4",
			"extension_group": "test_extension_4_group"
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
			}, {
				"name": "set_original_graph_info",
				"dest": [{
					"extension": "test_extension_4"
				}]
			}]
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

			// Send a 'set_original_graph_info' command to the specified
			// extension in the newly created graph
			cmdSetOriginalGraphInfo, _ := ten.NewCmd("set_original_graph_info")
			cmdSetOriginalGraphInfo.SetPropertyString(
				"original_graph_receiver_extension",
				"test_extension_1",
			)

			// Set destination to the test_extension_2 in the new graph
			cmdSetOriginalGraphInfo.SetDests(ten.Loc{
				AppURI:        ten.Ptr(""),
				GraphID:       ten.Ptr(newGraphID),
				ExtensionName: ten.Ptr("test_extension_2"),
			})

			tenEnv.SendCmd(
				cmdSetOriginalGraphInfo,
				func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
					// Send start command to test_extension_2
					cmdStart, _ := ten.NewCmd("start")
					cmdStart.SetDests(ten.Loc{
						AppURI:        ten.Ptr(""),
						GraphID:       ten.Ptr(newGraphID),
						ExtensionName: ten.Ptr("test_extension_2"),
					})

					tenEnv.SendCmd(
						cmdStart,
						func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
							// Stop the graph after processing
							stopGraphCmd, _ := ten.NewStopGraphCmd()
							stopGraphCmd.SetDests(ten.Loc{
								AppURI:        ten.Ptr(""),
								GraphID:       nil,
								ExtensionName: nil,
							})
							stopGraphCmd.SetGraphID(newGraphID)

							tenEnv.SendCmd(
								stopGraphCmd,
								func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
									ext.startAndStopGraphIsCompleted = true

									if ext.testCmd != nil &&
										ext.receivedDataFromNewGraph {
										ext.replyToClient(tenEnv)
									}
								},
							)
						},
					)
				},
			)
		},
	)

	tenEnv.OnStartDone()
}

func (ext *testExtension1) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	cmdName, _ := cmd.GetName()

	if cmdName == "test" {
		ext.testCmd = cmd

		if ext.startAndStopGraphIsCompleted && ext.receivedDataFromNewGraph {
			ext.replyToClient(tenEnv)
		}
	} else {
		panic("Should not happen - unknown command: " + cmdName)
	}
}

func (ext *testExtension1) OnData(tenEnv ten.TenEnv, data ten.Data) {
	dataName, _ := data.GetName()

	if dataName == "data_from_new_graph" {
		ext.receivedDataFromNewGraph = true

		if ext.testCmd != nil && ext.startAndStopGraphIsCompleted {
			ext.replyToClient(tenEnv)
		}
	} else {
		panic("Should not happen - unknown data: " + dataName)
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
