//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// test_extension_3: fast stopper in the dynamic graph.
//
// Calls on_stop_done immediately. With sync_stop_before_deinit enabled it
// cannot proceed to on_deinit until test_extension_2 (the slow stopper) also
// calls on_stop_done — which only happens after the 2-second sleep and the
// on_stop_notify round-trip.
//

package test_extension_3

import (
	ten "ten_framework/ten_runtime"
)

type testExtension3 struct {
	ten.DefaultExtension
}

func (ext *testExtension3) OnStop(tenEnv ten.TenEnv) {
	tenEnv.LogInfo("on_stop: calling on_stop_done immediately")
	tenEnv.OnStopDone()
}

func newTestExtension3(name string) ten.Extension {
	return &testExtension3{}
}

func init() {
	if err := ten.RegisterAddonAsExtension(
		"test_extension_3",
		ten.NewDefaultExtensionAddon(newTestExtension3),
	); err != nil {
		panic("Failed to register addon: " + err.Error())
	}
}
