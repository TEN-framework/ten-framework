//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"fmt"

	ten "ten_framework/ten_runtime"
)

type extensionB struct {
	ten.DefaultExtension
}

func newExtensionB(name string) ten.Extension {
	return &extensionB{}
}

func (p *extensionB) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		fmt.Println("extensionB OnCmd")

		cmdName, _ := cmd.GetName()
		if cmdName == "B" {
			// Try to get nonexistent property.
			res, err := cmd.GetPropertyString("agora")
			if err != nil {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
				cmdResult.SetPropertyString("detail", err.Error())
				tenEnv.ReturnResult(cmdResult, nil)
			} else {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
				cmdResult.SetPropertyString("detail", res)
				tenEnv.ReturnResult(cmdResult, nil)
			}

		} else {
		}
	}()
}

func init() {
	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_b",
		ten.NewDefaultExtensionAddon(newExtensionB),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
