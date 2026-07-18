//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// test_extension_2: slow stopper in the dynamic graph.
//
// In on_stop it sleeps for 2 seconds to simulate a heavy cleanup task, then
// sends an "on_stop_notify" command to the outside world (test_extension_1).
// It calls on_stop_done only after receiving the acknowledgement, so the
// test proves that:
//   1. Messages can be sent and received across graph boundaries during on_stop.
//   2. With sync_stop_before_deinit enabled, test_extension_3 (the fast
//      stopper) is forced to wait for this extension's on_stop_done before
//      either extension can proceed to on_deinit.
//

package test_extension_2

import (
	"time"

	ten "ten_framework/ten_runtime"
)

type testExtension2 struct {
	ten.DefaultExtension
}

func (ext *testExtension2) OnStop(tenEnv ten.TenEnv) {
	tenEnv.LogInfo("on_stop: sleeping 2s before sending on_stop_notify")

	// Block for 2 seconds inside on_stop to make timing visible in logs.
	time.Sleep(2 * time.Second)

	tenEnv.LogInfo("on_stop: sending on_stop_notify after 2s sleep")

	notifyCmd, _ := ten.NewCmd("on_stop_notify")

	tenEnv.SendCmd(
		notifyCmd,
		func(tenEnv ten.TenEnv, cmdResult ten.CmdResult, err error) {
			if err != nil {
				tenEnv.LogError("on_stop_notify failed: " + err.Error())
			} else {
				tenEnv.LogInfo("on_stop_notify ack received, calling on_stop_done")
			}
			tenEnv.OnStopDone()
		},
	)
}

func newTestExtension2(name string) ten.Extension {
	return &testExtension2{}
}

func init() {
	if err := ten.RegisterAddonAsExtension(
		"test_extension_2",
		ten.NewDefaultExtensionAddon(newTestExtension2),
	); err != nil {
		panic("Failed to register addon: " + err.Error())
	}
}
