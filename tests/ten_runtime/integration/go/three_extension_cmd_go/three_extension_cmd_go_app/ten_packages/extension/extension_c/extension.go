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
	"encoding/json"
	"fmt"

	ten "ten_framework/ten_runtime"
)

type userData struct {
	Result int `json:"result"`
}

type cExtension struct {
	ten.DefaultExtension
}

func NewExtensionC(name string) ten.Extension {
	return &cExtension{}
}

func (p *cExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		cmdName, _ := cmd.GetName()
		fmt.Println(
			"bExtension receive  command: ",
			cmdName,
		)

		if cmdName == "C" {
			data, err := cmd.GetPropertyInt64("data")
			if err != nil {
				panic("Should not happen.")
			}

			result := int(data) * 5

			res, _ := json.Marshal(userData{Result: result})

			fmt.Println("return command C, res:", string(res))

			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult.SetPropertyString("detail", string(res))
			err = tenEnv.ReturnResult(cmdResult, nil)
			if err != nil {
				panic("Should not happen.")
			}

		} else {
		}
	}()
}

func init() {
	fmt.Println("call init")

	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_c",
		ten.NewDefaultExtensionAddon(NewExtensionC),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
