// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to https://github.com/TEN-framework/ten_framework/LICENSE for more
// information.
//
// Note that this is just an example extension written in the GO programming
// language, so the package name does not equal to the containing directory
// name. However, it is not common in Go.
package default_extension_go

import (
	"fmt"

	"go_common_dep/types"
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
			data := types.UserData{Uid: 1, Name: "ten"}
			cs, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cs.SetProperty("data", &data)
			err := tenEnv.ReturnResult(cs, nil)
			if err != nil {
				panic(err)
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
