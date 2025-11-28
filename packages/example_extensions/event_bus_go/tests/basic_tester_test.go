//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package tests

import (
	ten "ten_framework/ten_runtime"
	"testing"
)

func TestBasicExtensionTester(t *testing.T) {
	myTester := &BasicExtensionTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.FailNow()
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestHighFrequencyTester tests high frequency command sending to detect
// potential segfaults.
func TestHighFrequencyTester(t *testing.T) {
	myTester := &HighFrequencyTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestBurstTester tests burst sending pattern to stress test the system.
func TestBurstTester(t *testing.T) {
	myTester := &BurstTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestRapidFireTester tests rapid fire sending with immediate callback
// processing.
func TestRapidFireTester(t *testing.T) {
	myTester := &RapidFireTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestMergeLogicTester tests result merging with multiple partial results.
func TestMergeLogicTester(t *testing.T) {
	myTester := &MergeLogicTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestErrorHandlingTester tests error handling with non-OK status codes.
func TestErrorHandlingTester(t *testing.T) {
	myTester := &ErrorHandlingTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestLongRunningTester tests continuous message sending for 5 minutes with
// result merging. This is a long-running test, use -timeout flag to extend test
// timeout if needed.
func TestLongRunningTester(t *testing.T) {
	myTester := &LongRunningTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}
