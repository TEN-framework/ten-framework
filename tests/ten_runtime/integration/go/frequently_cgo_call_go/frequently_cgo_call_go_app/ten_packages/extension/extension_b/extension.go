//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"fmt"
	"sync/atomic"

	ten "ten_framework/ten_runtime"
)

const concurrency = 1000

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
			var count uint32 = 0

			// An empty string in cmd is permitted.
			if em, err := cmd.GetPropertyString("empty_string"); err != nil ||
				em != "" {
				panic("Should not happen.")
			}

			if em, err := cmd.GetPropertyBytes("some_bytes"); err != nil ||
				len(em) != 3 {
				panic("Should not happen.")
			}

			done := make(chan struct{}, 1)
			defer close(done)

			for i := 0; i < concurrency; i++ {
				go func(i int) {
					res, err := cmd.GetPropertyInt64(
						fmt.Sprintf("prop_%d", i),
					)
					if err != nil || int(res) != i {
						panic("should not happen")
					}

					cmdName, _ := cmd.GetName()
					if cmdName != "B" {
						panic("should not happen")
					}

					total := atomic.AddUint32(&count, 1)
					if total%5000 == 0 {
						fmt.Printf("extension_b %d goroutine done\n", total)
					}

					if total == concurrency {
						done <- struct{}{}
					}
				}(i % 100)
			}
			<-done

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
		} else {
			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
			cmdResult.SetPropertyString("detail", "wrong cmd name")
			tenEnv.ReturnResult(cmdResult, nil)
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
