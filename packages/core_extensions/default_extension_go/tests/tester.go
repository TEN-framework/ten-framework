package tests

import (
	ten "ten_framework/ten_runtime"
)

type MyExtensionTester struct {
	ten.DefaultExtensionTester
}

func (tester *MyExtensionTester) OnStart(tenEnvTester ten.TenEnvTester) {
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

func (tester *MyExtensionTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")

	tenEnvTester.OnStopDone()
}
