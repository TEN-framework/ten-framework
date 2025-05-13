package tests

import (
	ten "ten_framework/ten_runtime"
)

type DataExtensionTester struct {
	ten.DefaultExtensionTester
}

func (tester *DataExtensionTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.OnStartDone()

	tenEnvTester.LogInfo("OnStart")

	dataTest, _ := ten.NewData("test")
	tenEnvTester.SendData(dataTest, func(tet ten.TenEnvTester, err error) {
		if err != nil {
			panic(err)
		}

		tenEnvTester.StopTest()
	})
}

func (tester *DataExtensionTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")

	tenEnvTester.OnStopDone()
}
