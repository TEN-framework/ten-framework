//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package tests

import (
	"fmt"
	ten "ten_framework/ten_runtime"
)

// GreetingTester is a tester for the Greeting extension.
type GreetingTester struct {
	ten.DefaultExtensionTester

	ExpectedGreetingMsg string
}

// OnStart is called when the test starts.
func (tester *GreetingTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")
	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *GreetingTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")
	tenEnvTester.OnStopDone()
}

// OnCmd is called when a cmd is received.
func (tester *GreetingTester) OnCmd(
	tenEnv ten.TenEnvTester,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	tenEnv.LogInfo(fmt.Sprintf("OnCmd: %s", cmdName))

	if cmdName == "greeting" {
		actualGreetingMsg, _ := cmd.GetPropertyString("greetingMsg")
		if actualGreetingMsg != tester.ExpectedGreetingMsg {
			panic(
				fmt.Sprintf(
					"Expected greeting message: %s, but got: %s",
					tester.ExpectedGreetingMsg,
					actualGreetingMsg,
				),
			)
		}

		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		tenEnv.ReturnResult(cmdResult, nil)

		tenEnv.StopTest()
	}
}
