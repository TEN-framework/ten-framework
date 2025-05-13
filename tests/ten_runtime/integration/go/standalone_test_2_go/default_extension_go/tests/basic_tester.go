package tests

import (
	ten "ten_framework/ten_runtime"
)

type BasicExtensionTester struct {
	ten.DefaultExtensionTester
}

func (tester *BasicExtensionTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.OnStartDone()

	tenEnvTester.LogInfo("OnStart")

	cmdTest, _ := ten.NewCmd("test")
	tenEnvTester.SendCmd(cmdTest, func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
		if err != nil {
			panic(err)
		}

		statusCode, _ := cr.GetStatusCode()
		if statusCode != ten.StatusCodeOk {
			panic(statusCode)
		}

		tenEnvTester.StopTest()
	})
}

func (tester *BasicExtensionTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")

	tenEnvTester.OnStopDone()
}
