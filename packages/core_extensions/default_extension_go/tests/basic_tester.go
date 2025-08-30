//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package tests

import (
	ten "ten_framework/ten_runtime"
)

// BasicExtensionTester is a tester for the basic extension.
type BasicExtensionTester struct {
	ten.DefaultExtensionTester
}

// OnStart is called when the test starts.
func (tester *BasicExtensionTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	cmdTest, _ := ten.NewCmd("test")
	tenEnvTester.SendCmd(
		cmdTest,
		func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
			if err != nil {
				panic(err)
			}

			statusCode, _ := cr.GetStatusCode()
			if statusCode != ten.StatusCodeOk {
				panic(statusCode)
			}

			tenEnvTester.StopTest(nil)
		},
	)

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *BasicExtensionTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")

	tenEnvTester.OnStopDone()
}
