//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"fmt"
	"time"

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
		tenEnv.SendCmd(cmdB, func(r ten.TenEnv, cs ten.CmdResult, e error) {
			detail, err := cs.GetPropertyPtr("data")
			if err != nil {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
				cmdResult.SetPropertyString("detail", err.Error())
				tenEnv.ReturnResult(cmdResult, nil)
			} else {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
				cmdResult.SetPropertyString("detail", detail.(*types.UserData).Name)
				tenEnv.ReturnResult(cmdResult, nil)
			}
		})

		time.Sleep(time.Second * 3)

		// Use the expired tenEnv object.
		newCmdB, _ := ten.NewCmd("B")
		err := tenEnv.SendCmd(newCmdB, nil)
		if err != nil {
			fmt.Println("Failed to use invalid ten object.")
		} else {
			panic("should not happen")
		}
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
