//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package default_extension_go

import (
	"fmt"

	ten "ten_framework/ten_runtime"
)

type defaultExtension struct {
	ten.DefaultExtension
}

func newExtension(name string) ten.Extension {
	return &defaultExtension{}
}

func (e *defaultExtension) OnStart(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("OnStart")

	tenEnv.OnStartDone()
}

func (e *defaultExtension) OnStop(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("OnStop")

	tenEnv.OnStopDone()
}

func (e *defaultExtension) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	tenEnv.LogDebug("OnCmd")

	cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	tenEnv.ReturnResult(cmdResult, nil)
}

func init() {
	fmt.Println("defaultExtension init")

	// Register addon
	ten.RegisterAddonAsExtension(
		"default_extension_go",
		ten.NewDefaultExtensionAddon(newExtension),
	)
}
