//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// Package default_extension_go is an example extension written in the GO
// programming language, so the package name does not equal to the containing
// directory name. However, it is not common in Go.
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

		srcLoc, err := cmd.GetSource()
		if err != nil {
			fmt.Println("Failed to get cmd source", err)
			panic(err)
		}
		fmt.Println("GetSource: appURI", *srcLoc.AppURI)
		fmt.Println("GetSource: graphID", *srcLoc.GraphID)
		fmt.Println("GetSource: extensionName", *srcLoc.ExtensionName)

		cmdName, _ := cmd.GetName()
		if cmdName == "B" {
			statusCmd, err := ten.NewCmdResult(
				ten.StatusCodeOk,
				cmd,
			)
			if err != nil {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
				cmdResult.SetPropertyString("detail", err.Error())
				tenEnv.ReturnResult(cmdResult, nil)
				return
			}

			statusCmd.SetProperty("detail", "this is extensionB.")
			statusCmd.SetProperty("password", "password")
			tenEnv.ReturnResult(statusCmd, nil)
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
