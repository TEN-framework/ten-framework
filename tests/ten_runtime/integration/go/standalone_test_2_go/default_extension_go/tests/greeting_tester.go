package tests

import (
	"fmt"
	ten "ten_framework/ten_runtime"
)

type GreetingTester struct {
	ten.DefaultExtensionTester

	ExpectedGreetingMsg string
}

func (tester *GreetingTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")
	tenEnvTester.OnStartDone()
}

func (tester *GreetingTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")
	tenEnvTester.OnStopDone()
}

func (tester *GreetingTester) OnCmd(
	tenEnv ten.TenEnvTester,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	tenEnv.LogInfo(fmt.Sprintf("OnCmd: %s", cmdName))

	if cmdName == "greeting" {
		actualGreetingMsg, _ := cmd.GetPropertyString("greetingMsg")
		if actualGreetingMsg != tester.ExpectedGreetingMsg {
			panic(fmt.Sprintf("Expected greeting message: %s, but got: %s", tester.ExpectedGreetingMsg, actualGreetingMsg))
		}

		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		tenEnv.ReturnResult(cmdResult, nil)

		tenEnv.StopTest()
	}
}
