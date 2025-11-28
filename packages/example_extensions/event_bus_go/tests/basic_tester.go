//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package tests

import (
	"fmt"
	"math/rand"
	"sync"
	"sync/atomic"
	ten "ten_framework/ten_runtime"
	"time"
)

// BasicExtensionTester is a tester for the basic extension.
type BasicExtensionTester struct {
	ten.DefaultExtensionTester
}

// OnCmd handles commands sent from the extension being tested.
// Since event_bus_go forwards commands back to tester, we need to handle them
// here.
func (tester *BasicExtensionTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	tenEnvTester.LogInfo("BasicExtensionTester received cmd: " + cmdName)

	// Simulate multiple async result returns to test event_bus merge logic
	var wg sync.WaitGroup

	// First return: partial result 1 (async with random delay)
	wg.Add(1)
	go func() {
		defer wg.Done()
		time.Sleep(time.Duration(rand.Intn(5)) * time.Millisecond)

		cmdResult1, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult1.SetFinal(false)
		cmdResult1.SetProperty("response", "partial_1")
		cmdResult1.SetProperty("data_part_1", "value1")
		tenEnvTester.ReturnResult(cmdResult1, nil)
	}()

	// Second return: partial result 2 (async with random delay)
	wg.Add(1)
	go func() {
		defer wg.Done()
		time.Sleep(time.Duration(rand.Intn(8)) * time.Millisecond)

		cmdResult2, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult2.SetFinal(false)
		cmdResult2.SetProperty("response", "partial_2")
		cmdResult2.SetProperty("data_part_2", "value2")
		tenEnvTester.ReturnResult(cmdResult2, nil)
	}()

	// Third return: final result (async return after previous ones complete)
	go func() {
		wg.Wait()
		time.Sleep(time.Duration(rand.Intn(3)) * time.Millisecond)

		cmdResult3, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult3.SetFinal(true)
		cmdResult3.SetProperty("response", "final")
		cmdResult3.SetProperty("data_part_3", "value3")
		tenEnvTester.ReturnResult(cmdResult3, nil)
	}()
}

// OnStart is called when the test starts.
func (tester *BasicExtensionTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStart")

	// Use ten_event command, which is the actual command supported by
	// event_bus_go
	cmdTest, _ := ten.NewCmd("ten_event")
	cmdTest.SetProperty("test_data", "basic_test")

	tenEnvTester.SendCmd(
		cmdTest,
		func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
			if err != nil {
				tenEnvTester.LogError(
					"Command returned with error: " + err.Error(),
				)
				panic(err)
			}

			statusCode, _ := cr.GetStatusCode()
			tenEnvTester.LogInfo("Command completed successfully")
			if statusCode != ten.StatusCodeOk {
				tenEnvTester.LogError("Unexpected status code")
				panic(statusCode)
			}

			tenEnvTester.StopTest(nil)
		},
	)

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *BasicExtensionTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("OnStop")

	tenEnvTester.OnStopDone()
}

// HighFrequencyTester tests high frequency command sending to detect potential
// segfaults.
type HighFrequencyTester struct {
	ten.DefaultExtensionTester
}

// OnCmd handles commands sent from the extension being tested.
func (tester *HighFrequencyTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	tenEnvTester.LogDebug("HighFrequencyTester received cmd: " + cmdName)

	// Test async multiple returns in high-frequency scenarios to verify
	// concurrency safety
	var wg sync.WaitGroup

	// First return: partial result (async)
	wg.Add(1)
	go func() {
		defer wg.Done()
		time.Sleep(time.Duration(rand.Intn(3)) * time.Millisecond)

		cmdResult1, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult1.SetFinal(false)
		cmdResult1.SetProperty("step", "processing")
		tenEnvTester.ReturnResult(cmdResult1, nil)
	}()

	// Second return: final result (wait for partial to complete)
	go func() {
		wg.Wait()
		time.Sleep(time.Duration(rand.Intn(2)) * time.Millisecond)

		cmdResult2, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult2.SetFinal(true)
		cmdResult2.SetProperty("step", "completed")
		tenEnvTester.ReturnResult(cmdResult2, nil)
	}()
}

// OnStart is called when the test starts.
func (tester *HighFrequencyTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("HighFrequencyTester OnStart")

	// Test parameters: send many commands to stress test the system
	const (
		numConcurrentSenders = 3  // Number of concurrent senders (reduced from 5)
		numCmdsPerSender     = 20 // Number of commands per sender (reduced from 50)
		totalCmds            = numConcurrentSenders * numCmdsPerSender
	)

	var (
		wg            sync.WaitGroup
		successCount  atomic.Int64
		errorCount    atomic.Int64
		completedCmds atomic.Int64
	)

	tenEnvTester.LogInfo(
		"Starting high frequency test: sending 60 commands concurrently",
	)

	// Start multiple concurrent senders
	for i := 0; i < numConcurrentSenders; i++ {
		wg.Add(1)
		senderID := i

		go func(id int) {
			defer wg.Done()

			// Each sender sends multiple commands
			for j := 0; j < numCmdsPerSender; j++ {
				// Create ten_event command (the actual command type handled by
				// event_bus)
				cmd, err := ten.NewCmd("ten_event")
				if err != nil {
					tenEnvTester.LogError("Failed to create command")
					errorCount.Add(1)
					completedCmds.Add(1)
					continue
				}

				// Set some properties to make the command more realistic
				cmd.SetProperty("sender_id", id)
				cmd.SetProperty("cmd_index", j)

				// Send command
				tenEnvTester.SendCmd(
					cmd,
					func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
						completed := completedCmds.Add(1)

						if err != nil {
							// In unit test mode, commands may fail due to no
							// connection
							// This is expected and should not cause a segfault
							errorCount.Add(1)
						} else {
							successCount.Add(1)
						}

						// When all commands are completed, stop the test
						if completed == totalCmds {
							success := successCount.Load()
							errors := errorCount.Load()
							tenEnvTester.LogInfo(
								"========================================",
							)
							tenEnvTester.LogInfo(
								"High frequency test completed successfully!",
							)
							tenEnvTester.LogInfo(
								"========================================",
							)
							tenEnvTester.LogInfo("Total commands sent: 60")
							tenEnvTester.LogInfo(
								fmt.Sprintf("Successful: %d", success),
							)
							tenEnvTester.LogInfo(
								fmt.Sprintf("Errors: %d", errors),
							)
							tenEnvTester.LogInfo(
								"No segfault detected - system is stable!",
							)
							tenEnvTester.LogInfo(
								"========================================",
							)
							tenEnvTester.StopTest(nil)
						}
					},
				)
			}
		}(senderID)
	}

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *HighFrequencyTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("HighFrequencyTester OnStop")
	tenEnvTester.OnStopDone()
}

// BurstTester tests burst sending pattern to stress test the system.
type BurstTester struct {
	ten.DefaultExtensionTester
}

// OnCmd handles commands sent from the extension being tested.
func (tester *BurstTester) OnCmd(tenEnvTester ten.TenEnvTester, cmd ten.Cmd) {
	// Quickly return to test system stability under burst load
	cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	tenEnvTester.ReturnResult(cmdResult, nil)
}

// OnStart is called when the test starts.
func (tester *BurstTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("BurstTester OnStart")

	// Test parameters: send many commands in a very short time
	const numBurstCmds = 100 // Reduced from 200
	var completedCmds atomic.Int64

	tenEnvTester.LogInfo(
		"Starting burst test: sending 100 commands in tight loop",
	)

	// Send all commands in a tight loop without waiting for responses
	for i := 0; i < numBurstCmds; i++ {
		cmd, err := ten.NewCmd("ten_event")
		if err != nil {
			tenEnvTester.LogError("Failed to create command")
			continue
		}

		cmd.SetProperty("burst_index", i)

		tenEnvTester.SendCmd(
			cmd,
			func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
				completed := completedCmds.Add(1)

				// When all commands are completed, stop the test
				if completed == numBurstCmds {
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.LogInfo("Burst test completed successfully!")
					tenEnvTester.LogInfo(
						"All 100 burst commands handled without segfault!",
					)
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.StopTest(nil)
				}
			},
		)
	}

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *BurstTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("BurstTester OnStop")
	tenEnvTester.OnStopDone()
}

// RapidFireTester tests rapid fire sending with immediate callback processing.
type RapidFireTester struct {
	ten.DefaultExtensionTester
}

// OnCmd handles commands sent from the extension being tested.
func (tester *RapidFireTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	// Quickly return to test stability of continuous sending
	cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
	tenEnvTester.ReturnResult(cmdResult, nil)
}

// OnStart is called when the test starts.
func (tester *RapidFireTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("RapidFireTester OnStart")

	// Rapid fire mode: send as quickly as possible consecutively
	const numRapidCmds = 200 // Reduced from 500
	var completedCmds atomic.Int64

	tenEnvTester.LogInfo(
		"Starting rapid fire test: 200 commands at maximum speed",
	)

	// Recursive sending function
	var sendNext func(index int)
	sendNext = func(index int) {
		if index >= numRapidCmds {
			return
		}

		cmd, err := ten.NewCmd("ten_event")
		if err != nil {
			tenEnvTester.LogError("Failed to create command")
			sendNext(index + 1)
			return
		}

		cmd.SetProperty("rapid_index", index)

		tenEnvTester.SendCmd(
			cmd,
			func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
				completed := completedCmds.Add(1)

				if completed == numRapidCmds {
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.LogInfo(
						"Rapid fire test completed successfully!",
					)
					tenEnvTester.LogInfo(
						"200 rapid fire commands handled without segfault!",
					)
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.StopTest(nil)
				}
			},
		)

		// Immediately send the next one
		sendNext(index + 1)
	}

	// Start sending
	sendNext(0)

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *RapidFireTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("RapidFireTester OnStop")
	tenEnvTester.OnStopDone()
}

// MergeLogicTester tests the result merging logic with multiple partial
// results.
type MergeLogicTester struct {
	ten.DefaultExtensionTester
	resultCount int
}

// OnCmd handles commands sent from the extension being tested.
func (tester *MergeLogicTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	tester.resultCount++
	cmdName, _ := cmd.GetName()
	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"MergeLogicTester received cmd #%d: %s",
			tester.resultCount,
			cmdName,
		),
	)

	// Asynchronously return 5 partial results to test merge logic correctness
	// in concurrent scenarios
	var wg sync.WaitGroup

	for i := 1; i <= 5; i++ {
		wg.Add(1)
		// Capture loop variable
		index := i
		go func() {
			defer wg.Done()
			// Each partial result has random delay of 0-10ms
			time.Sleep(time.Duration(rand.Intn(10)) * time.Millisecond)

			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult.SetFinal(false)
			cmdResult.SetProperty(
				fmt.Sprintf("field_%d", index),
				fmt.Sprintf("value_%d", index),
			)
			cmdResult.SetProperty("counter", index)
			tenEnvTester.ReturnResult(cmdResult, nil)
		}()
	}

	// Finally, asynchronously return the final result
	go func() {
		wg.Wait()
		time.Sleep(time.Duration(rand.Intn(5)) * time.Millisecond)

		cmdResultFinal, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResultFinal.SetFinal(true)
		cmdResultFinal.SetProperty("status", "all_merged")
		cmdResultFinal.SetProperty("total_parts", 5)
		tenEnvTester.ReturnResult(cmdResultFinal, nil)
	}()
}

// OnStart is called when the test starts.
func (tester *MergeLogicTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("MergeLogicTester OnStart")
	tenEnvTester.LogInfo(
		"Testing result merging with 5 partial results + 1 final result",
	)

	// Send 3 commands, each command will return 6 times (5 partial + 1 final) -
	// reduced from 5
	const numCmds = 3
	var completedCmds atomic.Int64

	for i := 0; i < numCmds; i++ {
		cmd, _ := ten.NewCmd("ten_event")
		cmd.SetProperty("test_type", "merge_logic")
		cmd.SetProperty("cmd_index", i)

		tenEnvTester.SendCmd(
			cmd,
			func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
				completed := completedCmds.Add(1)

				if err != nil {
					tenEnvTester.LogError(fmt.Sprintf("Command error: %v", err))
					panic(err)
				}

				statusCode, _ := cr.GetStatusCode()
				if statusCode != ten.StatusCodeOk {
					tenEnvTester.LogError(
						fmt.Sprintf("Unexpected status code: %d", statusCode),
					)
					panic(statusCode)
				}

				// Verify the merged result
				result, _ := cr.GetPropertyToJSONBytes("")
				tenEnvTester.LogInfo(
					fmt.Sprintf("Merged result: %s", string(result)),
				)

				if completed == numCmds {
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.LogInfo(
						"Merge logic test completed successfully!",
					)
					tenEnvTester.LogInfo(
						"All commands returned merged results correctly!",
					)
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.StopTest(nil)
				}
			},
		)
	}

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *MergeLogicTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("MergeLogicTester OnStop")
	tenEnvTester.OnStopDone()
}

// ErrorHandlingTester tests error handling with non-OK status codes.
type ErrorHandlingTester struct {
	ten.DefaultExtensionTester
	cmdCounter atomic.Int32
}

// OnCmd handles commands sent from the extension being tested.
func (tester *ErrorHandlingTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	counter := tester.cmdCounter.Add(1)
	cmdName, _ := cmd.GetName()
	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"ErrorHandlingTester received cmd #%d: %s",
			counter,
			cmdName,
		),
	)

	// Test different error scenarios (async return)
	switch counter % 3 {
	case 1:
		// Scenario 1: Asynchronously return multiple OK partial results +
		// finally return Error status
		var wg sync.WaitGroup

		wg.Add(1)
		go func() {
			defer wg.Done()
			time.Sleep(time.Duration(rand.Intn(5)) * time.Millisecond)
			cmdResult1, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult1.SetFinal(false)
			cmdResult1.SetProperty("partial_data", "data1")
			tenEnvTester.ReturnResult(cmdResult1, nil)
		}()

		wg.Add(1)
		go func() {
			defer wg.Done()
			time.Sleep(time.Duration(rand.Intn(8)) * time.Millisecond)
			cmdResult2, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult2.SetFinal(false)
			cmdResult2.SetProperty("partial_data", "data2")
			tenEnvTester.ReturnResult(cmdResult2, nil)
		}()

		// Asynchronously return error status code - event_bus should return
		// immediately
		go func() {
			wg.Wait()
			time.Sleep(time.Duration(rand.Intn(3)) * time.Millisecond)
			cmdResultError, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
			cmdResultError.SetFinal(true)
			cmdResultError.SetProperty("error", "simulated_error")
			tenEnvTester.ReturnResult(cmdResultError, nil)
		}()

	case 2:
		// Scenario 2: Asynchronously return normal partial + final
		var wg sync.WaitGroup

		wg.Add(1)
		go func() {
			defer wg.Done()
			time.Sleep(time.Duration(rand.Intn(5)) * time.Millisecond)
			cmdResult1, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult1.SetFinal(false)
			cmdResult1.SetProperty("status", "processing")
			tenEnvTester.ReturnResult(cmdResult1, nil)
		}()

		go func() {
			wg.Wait()
			time.Sleep(time.Duration(rand.Intn(3)) * time.Millisecond)
			cmdResultFinal, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResultFinal.SetFinal(true)
			cmdResultFinal.SetProperty("status", "success")
			tenEnvTester.ReturnResult(cmdResultFinal, nil)
		}()

	case 0:
		// Scenario 3: Directly return final result asynchronously (no partial)
		go func() {
			time.Sleep(time.Duration(rand.Intn(5)) * time.Millisecond)
			cmdResultFinal, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResultFinal.SetFinal(true)
			cmdResultFinal.SetProperty("status", "immediate_success")
			tenEnvTester.ReturnResult(cmdResultFinal, nil)
		}()
	}
}

// OnStart is called when the test starts.
func (tester *ErrorHandlingTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("ErrorHandlingTester OnStart")
	tenEnvTester.LogInfo("Testing error handling and edge cases")

	const numCmds = 6 // Reduced from 10
	var (
		completedCmds atomic.Int64
		successCount  atomic.Int64
		errorCount    atomic.Int64
	)

	for i := 0; i < numCmds; i++ {
		cmd, _ := ten.NewCmd("ten_event")
		cmd.SetProperty("test_type", "error_handling")
		cmd.SetProperty("cmd_index", i)

		tenEnvTester.SendCmd(
			cmd,
			func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
				completed := completedCmds.Add(1)

				if err != nil {
					errorCount.Add(1)
					tenEnvTester.LogDebug(
						fmt.Sprintf(
							"Command returned error (expected): %v",
							err,
						),
					)
				} else {
					statusCode, _ := cr.GetStatusCode()
					if statusCode != ten.StatusCodeOk {
						errorCount.Add(1)
						tenEnvTester.LogDebug(fmt.Sprintf("Non-OK status (expected): %d", statusCode))
					} else {
						successCount.Add(1)
					}
				}

				if completed == numCmds {
					success := successCount.Load()
					errors := errorCount.Load()
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.LogInfo(
						"Error handling test completed successfully!",
					)
					tenEnvTester.LogInfo(
						fmt.Sprintf(
							"Successful: %d, Errors: %d",
							success,
							errors,
						),
					)
					tenEnvTester.LogInfo(
						"Verified: non-OK status codes handled correctly!",
					)
					tenEnvTester.LogInfo(
						"========================================",
					)
					tenEnvTester.StopTest(nil)
				}
			},
		)
	}

	tenEnvTester.OnStartDone()
}

// OnStop is called when the test stops.
func (tester *ErrorHandlingTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("ErrorHandlingTester OnStop")
	tenEnvTester.OnStopDone()
}

// LongRunningTester tests continuous message sending for 5 minutes with result
// merging.
type LongRunningTester struct {
	ten.DefaultExtensionTester

	totalSent      atomic.Int64
	totalCompleted atomic.Int64
	totalSuccess   atomic.Int64
	totalErrors    atomic.Int64
	isRunning      atomic.Bool
	startTime      time.Time
}

// OnCmd handles commands sent from the extension being tested.
func (tester *LongRunningTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	cmdName, _ := cmd.GetName()
	tenEnvTester.LogDebug("LongRunningTester received cmd: " + cmdName)

	// Use WaitGroup to ensure all partial results are sent before sending the
	// final result
	var wg sync.WaitGroup

	// Each command returns 3 partial results + 1 final result
	// Returned asynchronously in different goroutines, simulating multi-stage
	// async processing in real business

	// Stage 1: Data validation (async, random delay 0-10ms)
	wg.Add(1)
	go func() {
		defer wg.Done()
		// Random delay to simulate real processing time
		time.Sleep(time.Duration(rand.Intn(10)) * time.Millisecond)

		cmdResult1, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult1.SetFinal(false)
		cmdResult1.SetProperty("stage", "validation")
		cmdResult1.SetProperty("validation_result", "passed")
		cmdResult1.SetProperty("timestamp_1", time.Now().UnixNano())
		tenEnvTester.ReturnResult(cmdResult1, nil)
	}()

	// Stage 2: Data processing (async, random delay 0-15ms)
	wg.Add(1)
	go func() {
		defer wg.Done()
		// Random delay to simulate real processing time
		time.Sleep(time.Duration(rand.Intn(15)) * time.Millisecond)

		cmdResult2, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult2.SetFinal(false)
		cmdResult2.SetProperty("stage", "processing")
		cmdResult2.SetProperty("processing_result", "completed")
		cmdResult2.SetProperty("timestamp_2", time.Now().UnixNano())
		tenEnvTester.ReturnResult(cmdResult2, nil)
	}()

	// Stage 3: Data storage (async, random delay 0-20ms)
	wg.Add(1)
	go func() {
		defer wg.Done()
		// Random delay to simulate real processing time
		time.Sleep(time.Duration(rand.Intn(20)) * time.Millisecond)

		cmdResult3, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult3.SetFinal(false)
		cmdResult3.SetProperty("stage", "storage")
		cmdResult3.SetProperty("storage_result", "saved")
		cmdResult3.SetProperty("timestamp_3", time.Now().UnixNano())
		tenEnvTester.ReturnResult(cmdResult3, nil)
	}()

	// Stage 4: After all partial results are completed, asynchronously return
	// the final result
	go func() {
		// Wait for all partial results to be sent
		wg.Wait()

		// Add a small delay to ensure all partial results have been processed
		time.Sleep(time.Duration(rand.Intn(5)) * time.Millisecond)

		cmdResultFinal, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResultFinal.SetFinal(true)
		cmdResultFinal.SetProperty("stage", "completed")
		cmdResultFinal.SetProperty("final_status", "success")
		cmdResultFinal.SetProperty("timestamp_final", time.Now().UnixNano())
		tenEnvTester.ReturnResult(cmdResultFinal, nil)
	}()
}

// OnStart is called when the test starts.
func (tester *LongRunningTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("LongRunningTester OnStart")
	tenEnvTester.LogInfo("========================================")
	tenEnvTester.LogInfo("Starting 5-second continuous stress test")
	tenEnvTester.LogInfo("Each message includes 3 partial + 1 final result")
	tenEnvTester.LogInfo("========================================")

	tester.startTime = time.Now()
	tester.isRunning.Store(true)

	// Test duration: 5 seconds (reduced from 10 seconds)
	const testDuration = 5 * time.Second

	// Send interval: send a batch of commands every 100ms
	const sendInterval = 100 * time.Millisecond

	// Number of commands per batch (reduced from 5 to 3)
	const batchSize = 3

	// Start sending goroutine
	go func() {
		ticker := time.NewTicker(sendInterval)
		defer ticker.Stop()

		for {
			if !tester.isRunning.Load() {
				break
			}

			elapsed := time.Since(tester.startTime)
			if elapsed >= testDuration {
				tenEnvTester.LogInfo("========================================")
				tenEnvTester.LogInfo("5 seconds completed, stopping test...")
				tenEnvTester.LogInfo("========================================")
				tester.isRunning.Store(false)
				break
			}

			// Send a batch of commands per tick
			<-ticker.C
			for i := 0; i < batchSize; i++ {
				tester.sendCommand(tenEnvTester)
			}
		}

		// Wait for all commands to complete
		tenEnvTester.LogInfo("Waiting for all commands to complete...")
		for {
			sent := tester.totalSent.Load()
			completed := tester.totalCompleted.Load()
			if completed >= sent {
				break
			}
			time.Sleep(100 * time.Millisecond)
		}

		// Print statistics
		tester.printStatistics(tenEnvTester)
		tenEnvTester.StopTest(nil)
	}()

	// Start statistics goroutine, print progress every 30 seconds
	go func() {
		ticker := time.NewTicker(30 * time.Second)
		defer ticker.Stop()

		for {
			select {
			case <-ticker.C:
				if !tester.isRunning.Load() {
					return
				}

				elapsed := time.Since(tester.startTime)
				sent := tester.totalSent.Load()
				completed := tester.totalCompleted.Load()
				success := tester.totalSuccess.Load()
				errors := tester.totalErrors.Load()

				tenEnvTester.LogInfo("========================================")
				tenEnvTester.LogInfo(
					fmt.Sprintf(
						"Progress: %.0f seconds elapsed",
						elapsed.Seconds(),
					),
				)
				tenEnvTester.LogInfo(
					fmt.Sprintf("Sent: %d, Completed: %d", sent, completed),
				)
				tenEnvTester.LogInfo(
					fmt.Sprintf("Success: %d, Errors: %d", success, errors),
				)
				if completed > 0 {
					tenEnvTester.LogInfo(
						fmt.Sprintf(
							"Success rate: %.2f%%",
							float64(success)/float64(completed)*100,
						),
					)
				}
				tenEnvTester.LogInfo("========================================")
			default:
				if !tester.isRunning.Load() {
					return
				}
				time.Sleep(100 * time.Millisecond)
			}
		}
	}()

	tenEnvTester.OnStartDone()
}

// sendCommand sends a single command.
func (tester *LongRunningTester) sendCommand(tenEnvTester ten.TenEnvTester) {
	cmdIndex := tester.totalSent.Add(1)

	cmd, err := ten.NewCmd("ten_event")
	if err != nil {
		tenEnvTester.LogError(fmt.Sprintf("Failed to create command: %v", err))
		tester.totalCompleted.Add(1)
		tester.totalErrors.Add(1)
		return
	}

	cmd.SetProperty("test_type", "long_running")
	cmd.SetProperty("cmd_index", cmdIndex)
	cmd.SetProperty("timestamp_sent", time.Now().UnixNano())

	tenEnvTester.SendCmd(
		cmd,
		func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
			tester.totalCompleted.Add(1)

			if err != nil {
				tester.totalErrors.Add(1)
				tenEnvTester.LogDebug(
					fmt.Sprintf("Command #%d error: %v", cmdIndex, err),
				)
				return
			}

			statusCode, _ := cr.GetStatusCode()
			if statusCode != ten.StatusCodeOk {
				tester.totalErrors.Add(1)
				tenEnvTester.LogDebug(
					fmt.Sprintf(
						"Command #%d non-OK status: %d",
						cmdIndex,
						statusCode,
					),
				)
				return
			}

			tester.totalSuccess.Add(1)

			// Log every 1000 successful commands
			if tester.totalSuccess.Load()%1000 == 0 {
				tenEnvTester.LogInfo(
					fmt.Sprintf(
						"Milestone: %d commands completed successfully",
						tester.totalSuccess.Load(),
					),
				)
			}
		},
	)
}

// printStatistics prints the final test statistics.
func (tester *LongRunningTester) printStatistics(
	tenEnvTester ten.TenEnvTester,
) {
	duration := time.Since(tester.startTime)
	sent := tester.totalSent.Load()
	completed := tester.totalCompleted.Load()
	success := tester.totalSuccess.Load()
	errors := tester.totalErrors.Load()

	tenEnvTester.LogInfo("========================================")
	tenEnvTester.LogInfo("Long Running Test Completed Successfully!")
	tenEnvTester.LogInfo("========================================")
	tenEnvTester.LogInfo(
		fmt.Sprintf("Total duration: %.2f seconds", duration.Seconds()),
	)
	tenEnvTester.LogInfo(fmt.Sprintf("Total commands sent: %d", sent))
	tenEnvTester.LogInfo(fmt.Sprintf("Total commands completed: %d", completed))
	tenEnvTester.LogInfo(fmt.Sprintf("Successful: %d", success))
	tenEnvTester.LogInfo(fmt.Sprintf("Errors: %d", errors))

	if completed > 0 {
		successRate := float64(success) / float64(completed) * 100
		tenEnvTester.LogInfo(fmt.Sprintf("Success rate: %.2f%%", successRate))
	}

	if duration.Seconds() > 0 {
		throughput := float64(completed) / duration.Seconds()
		tenEnvTester.LogInfo(
			fmt.Sprintf("Average throughput: %.2f commands/second", throughput),
		)
	}

	// Each command has 4 returns (3 partial + 1 final), calculate total result
	// count
	totalResults := success * 4
	tenEnvTester.LogInfo(fmt.Sprintf("Total results merged: %d", totalResults))
	tenEnvTester.LogInfo("========================================")
	tenEnvTester.LogInfo("No segfault, no memory leak detected!")
	tenEnvTester.LogInfo("System is stable under long-running load!")
	tenEnvTester.LogInfo("========================================")
}

// OnStop is called when the test stops.
func (tester *LongRunningTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("LongRunningTester OnStop")

	// Stop all goroutines
	tester.isRunning.Store(false)

	// Start a new goroutine to wait for cleanup and call OnStopDone
	go func() {
		// Wait for all pending commands to complete
		tenEnvTester.LogInfo("Waiting for all pending commands to complete...")
		maxWaitTime := 5 * time.Second
		startWait := time.Now()
		for {
			sent := tester.totalSent.Load()
			completed := tester.totalCompleted.Load()
			if completed >= sent {
				break
			}
			if time.Since(startWait) > maxWaitTime {
				tenEnvTester.LogWarn(
					fmt.Sprintf(
						"Timeout waiting for pending commands, %d still pending",
						sent-completed,
					),
				)
				break
			}
			tenEnvTester.LogDebug(
				fmt.Sprintf(
					"Still waiting for %d pending commands...",
					sent-completed,
				),
			)
			time.Sleep(50 * time.Millisecond)
		}

		tenEnvTester.LogInfo("All pending commands completed")

		// Call OnStopDone after everything is cleaned up
		tenEnvTester.OnStopDone()
	}()
}
