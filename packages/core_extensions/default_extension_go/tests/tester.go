package tests

import (
	"fmt"
	ten "ten_framework/ten_runtime"
)

type MyExtensionTester struct {
	ten.DefaultExtensionTester
}

func (tester *MyExtensionTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.OnStartDone()

	fmt.Println("xxxxx OnStart")

	tenEnvTester.StopTest()
}

func (tester *MyExtensionTester) OnStop(tenEnvTester ten.TenEnvTester) {
	fmt.Println("xxxxx OnStop")

	tenEnvTester.OnStopDone()
}
