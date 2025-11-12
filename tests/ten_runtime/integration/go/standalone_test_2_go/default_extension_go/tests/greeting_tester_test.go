//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package tests

import (
	ten "ten_framework/ten_runtime"
	"testing"
	"time"
)

func TestGreetingTester(t *testing.T) {
	greetingMsg := "im ok!"

	myTester := &GreetingTester{
		ExpectedGreetingMsg: greetingMsg,
	}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle(
		"default_extension_go",
		"{\"greetingMsg\": \""+greetingMsg+"\"}",
	)
	tester.Run()
}

func TestGreetingTesterEmpty(t *testing.T) {
	myTester := &GreetingTester{
		ExpectedGreetingMsg: "",
	}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("default_extension_go", "{}")
	tester.Run()
}

func TestGreetingTesterFailure(t *testing.T) {
	myTester := &GreetingTester{
		ExpectedGreetingMsg: "im not ok!",
	}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle(
		"default_extension_go",
		"{\"greetingMsg\": \"im ok!\"}",
	)
	err = tester.Run()
	if err != nil {
		t.Logf("Expected error: %v", err)
	} else {
		t.FailNow()
	}
}

func TestGreetingTesterTimeout(t *testing.T) {
	myTester := &GreetingTester{
		DelayMs: 1000,
	}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTimeout(time.Duration(500) * time.Millisecond)
	tester.SetTestModeSingle("default_extension_go", "{}")
	err = tester.Run()
	if err != nil {
		tenErr, _ := err.(*ten.TenError)
		if tenErr.ErrorCode != ten.ErrorCodeTimeout {
			t.FailNow()
		}
		t.Logf("Expected error: %v", err)
	} else {
		t.FailNow()
	}
}
