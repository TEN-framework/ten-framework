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

type graphStarterExtension struct {
	ten.DefaultExtension
}

func (ext *graphStarterExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
	name, _ := cmd.GetName()
	if name == "start_graph" {
		startGraphCmd, _ := ten.NewStartGraphCmd()
		startGraphCmd.SetPredefinedGraphName("biz")
		startGraphCmd.SetDests(ten.Loc{
			AppURI:        ten.Ptr(""),
			GraphID:       nil,
			ExtensionName: nil,
		})

		tenEnv.SendCmd(
			startGraphCmd,
			func(tenEnv ten.TenEnv, cr ten.CmdResult, err error) {
				if err != nil {
					panic("Failed to start graph: " + err.Error())
				}

				statusCode, _ := cr.GetStatusCode()
				if statusCode != ten.StatusCodeOk {
					panic("Failed to start graph")
				}

				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
				cmdResult.SetPropertyString("detail", "ok")
				tenEnv.ReturnResult(cmdResult, nil)
			},
		)
	} else {
		panic("unknown cmd name: " + name)
	}
}

func newAExtension(name string) ten.Extension {
	return &graphStarterExtension{}
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
