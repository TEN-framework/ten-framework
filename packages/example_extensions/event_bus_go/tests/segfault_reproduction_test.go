package tests

import (
	"testing"

	ten "ten_framework/ten_runtime"
)

// TestHighLoadWithStreaming - Main test: High load + streaming data
// This test is most similar to the segfault scenario in the logs
// Run: ./bin/start TestHighLoadWithStreaming
func TestHighLoadWithStreaming(t *testing.T) {
	myTester := &HighLoadWithStreamingTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestGCPressure - GC pressure test
// Because segfault occurred in runtime.mbitmap.go
// Run: ./bin/start TestGCPressure
func TestGCPressure(t *testing.T) {
	myTester := &GCPressureTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}

// TestConcurrentCallback - Concurrent callback test
// Test thread safety of hasReturnedResult
// Run: ./bin/start TestConcurrentCallback
func TestConcurrentCallback(t *testing.T) {
	myTester := &ConcurrentCallbackTester{}

	tester, err := ten.NewExtensionTester(myTester)
	if err != nil {
		t.Fatalf("Failed to create extension tester: %v", err)
	}

	tester.SetTestModeSingle("event_bus_go", "{}")
	tester.Run()
}
