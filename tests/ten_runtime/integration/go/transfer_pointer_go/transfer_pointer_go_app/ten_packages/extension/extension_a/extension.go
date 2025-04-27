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

type extensionA struct {
	ten.DefaultExtension
}

func newExtensionA(name string) ten.Extension {
	return &extensionA{}
}

func (p *extensionA) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		fmt.Println("extensionA OnCmd")

		cmdB, _ := ten.NewCmd("B")

		strArray := []string{"hello", "world", "ten"}
		cmdB.SetProperty("array", &strArray)

		propMap := make(map[string]interface{})
		propMap["paramA"] = "A"
		cmdB.SetProperty("map", &propMap)

		data := &types.UserData{Uid: 2, Name: "str"}
		cmdB.SetProperty("struct", data)
		data.Uid = 3

		tenEnv.SendCmd(cmdB, func(r ten.TenEnv, cs ten.CmdResult, e error) {
			detail, err := cs.GetPropertyString("detail")
			if err != nil {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
				cmdResult.SetPropertyString("detail", err.Error())
				r.ReturnResult(cmdResult, nil)
				return
			}

			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult.SetPropertyString("detail", detail)
			r.ReturnResult(cmdResult, nil)
		})
	}()
}

func init() {
	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_a",
		ten.NewDefaultExtensionAddon(newExtensionA),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
