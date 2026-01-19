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
	"time"
)

// GreetingTester is a tester for the Greeting extension.
type GreetingTester struct {
	ten.DefaultExtensionTester

	ExpectedGreetingMsg string
	DelayMs             uint32
}

// OnStart is called when the test starts.
func (tester *GreetingTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	// Test log with fields in OnStart
	startFields := ten.NewObjectValue(map[string]ten.Value{
		"tester_name":      ten.NewStringValue("GreetingTester"),
		"expected_message": ten.NewStringValue(tester.ExpectedGreetingMsg),
		"delay_ms":         ten.NewUint32Value(tester.DelayMs),
		"test_phase":       ten.NewStringValue("start"),
	})
	startCategory := "test_lifecycle"
	if err := tenEnvTester.Log(ten.LogLevelInfo, "Test started with configuration", &startCategory, &startFields, nil); err != nil {
		panic("Log with fields failed: " + err.Error())
	}

	if tester.DelayMs > 0 {
		time.Sleep(time.Duration(tester.DelayMs) * time.Millisecond)
	}

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *GreetingTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")

	// Test log with fields in OnStop
	stopFields := ten.NewObjectValue(map[string]ten.Value{
		"tester_name": ten.NewStringValue("GreetingTester"),
		"test_phase":  ten.NewStringValue("stop"),
		"status":      ten.NewStringValue("stopping"),
	})
	stopCategory := "test_lifecycle"
	if err := tenEnvTester.Log(ten.LogLevelInfo, "Test stopping", &stopCategory, &stopFields, nil); err != nil {
		panic("Log with fields failed: " + err.Error())
	}

	tenEnvTester.OnStopDone()
}

// OnCmd is called when a cmd is received.
func (tester *GreetingTester) OnCmd(
	tenEnv ten.TenEnvTester,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	tenEnv.LogInfo(fmt.Sprintf("OnCmd: %s", cmdName))

	// Test log with fields containing command information
	cmdFields := ten.NewObjectValue(map[string]ten.Value{
		"cmd_name":         ten.NewStringValue(cmdName),
		"tester_name":      ten.NewStringValue("GreetingTester"),
		"test_phase":       ten.NewStringValue("command_processing"),
		"has_expected_msg": ten.NewBoolValue(tester.ExpectedGreetingMsg != ""),
	})
	cmdCategory := "command"
	if err := tenEnv.Log(ten.LogLevelDebug, "Processing command with fields", &cmdCategory, &cmdFields, nil); err != nil {
		panic("Log with fields failed: " + err.Error())
	}

	if cmdName == "greeting" {
		actualGreetingMsg, _ := cmd.GetPropertyString("greetingMsg")

		// Determine validation result
		validationResult := "failed"
		if actualGreetingMsg == tester.ExpectedGreetingMsg {
			validationResult = "passed"
		}

		// Test log with fields containing validation information
		validationFields := ten.NewObjectValue(map[string]ten.Value{
			"cmd_name":          ten.NewStringValue(cmdName),
			"expected_message":  ten.NewStringValue(tester.ExpectedGreetingMsg),
			"actual_message":    ten.NewStringValue(actualGreetingMsg),
			"messages_match":    ten.NewBoolValue(actualGreetingMsg == tester.ExpectedGreetingMsg),
			"validation_result": ten.NewStringValue(validationResult),
		})
		validationCategory := "validation"
		if err := tenEnv.Log(ten.LogLevelInfo, "Validating greeting message", &validationCategory, &validationFields, nil); err != nil {
			panic("Log with fields failed: " + err.Error())
		}

		if actualGreetingMsg != tester.ExpectedGreetingMsg {
			// Test log with error fields
			errorFields := ten.NewObjectValue(map[string]ten.Value{
				"error_type": ten.NewStringValue("validation_error"),
				"expected":   ten.NewStringValue(tester.ExpectedGreetingMsg),
				"actual":     ten.NewStringValue(actualGreetingMsg),
				"error_code": ten.NewIntValue(int(ten.ErrorCodeGeneric)),
			})
			if err := tenEnv.Log(ten.LogLevelError, "Greeting message mismatch", nil, &errorFields, nil); err != nil {
				panic("Log with fields failed: " + err.Error())
			}

			tenEnv.StopTest(ten.NewTenError(ten.ErrorCodeGeneric,
				fmt.Sprintf(
					"Expected greeting message: %s, but got: %s",
					tester.ExpectedGreetingMsg,
					actualGreetingMsg,
				),
			))
			return
		}

		// Test log with success fields
		successFields := ten.NewObjectValue(map[string]ten.Value{
			"cmd_name":       ten.NewStringValue(cmdName),
			"status":         ten.NewStringValue("success"),
			"message":        ten.NewStringValue(actualGreetingMsg),
			"test_completed": ten.NewBoolValue(true),
		})
		successCategory := "test_result"
		if err := tenEnv.Log(ten.LogLevelInfo, "Test completed successfully", &successCategory, &successFields, nil); err != nil {
			panic("Log with fields failed: " + err.Error())
		}

		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		tenEnv.ReturnResult(cmdResult, nil)

		err := tenEnv.StopTest(nil)
		if err != nil {
			panic(err)
		}
	}
}
