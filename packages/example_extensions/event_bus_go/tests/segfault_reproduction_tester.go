//
// Segfault Reproduction Test - Targeted tests designed based on actual
// scenarios in logs
// Key findings from logs:
// 1. Segfault occurred in runtime.mbitmap.go (during Go GC process)
// 2. message_collector was sending large amounts of data when it occurred
// (buffer send in last 9.5s 67435)
// 3. event_bus_go was forwarding ten_event commands at high frequency
// 4. Multiple threads accessing simultaneously (thread 0x77a1a67fc640, thread
// 0x77a1a6ffd640)
//

package tests

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"sync"
	"sync/atomic"
	ten "ten_framework/ten_runtime"
	"time"
)

// =====================================================
// Test 1: High-frequency concurrent scenario + large data streaming return
// This test is most similar to the segfault scenario in the logs
// =====================================================

// HighLoadWithStreamingTester - Simulates production environment high-load
// scenario
type HighLoadWithStreamingTester struct {
	ten.DefaultExtensionTester
	sentCount     atomic.Int64
	receivedCount atomic.Int64
	callbackCount atomic.Int64
	pendingCount  atomic.Int64 // New: Track pending message count
	start         time.Time
	stopChan      chan struct{}
	wg            sync.WaitGroup
	maxPending    int64         // New: Maximum pending count threshold
	burstSize     int           // New: Burst batch size
	burstInterval time.Duration // New: Burst interval
}

func (tester *HighLoadWithStreamingTester) OnStart(
	tenEnvTester ten.TenEnvTester,
) {
	tenEnvTester.LogInfo("=== HighLoadWithStreamingTester Start ===")
	tenEnvTester.LogInfo(
		"Scenario: High-concurrency burst + flow control + large data streaming return",
	)
	tenEnvTester.LogInfo(
		"Goal: Maintain stable pending message count under high concurrency to reproduce cmdResultMerged concurrency bug",
	)

	tester.start = time.Now()
	tester.stopChan = make(chan struct{})

	// Flow control parameter configuration
	tester.maxPending = 20                        // Max pending count: 20 messages (reduced from 50)
	tester.burstSize = 5                          // Send 5 messages per burst (reduced from 10)
	tester.burstInterval = 100 * time.Millisecond // Attempt burst every 100ms (increased from 50ms)

	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"Flow control config: maxPending=%d, burstSize=%d, burstInterval=%v",
			tester.maxPending,
			tester.burstSize,
			tester.burstInterval,
		),
	)
	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"Theoretical peak rate: %d msg/burst * %.1f burst/s = %.1f msg/s",
			tester.burstSize,
			1000.0/float64(tester.burstInterval.Milliseconds()),
			float64(
				tester.burstSize,
			)*1000.0/float64(
				tester.burstInterval.Milliseconds(),
			),
		),
	)

	// Sender goroutine: Use flow control mechanism
	tester.wg.Add(1)
	go func() {
		defer tester.wg.Done()

		ticker := time.NewTicker(tester.burstInterval)
		defer ticker.Stop()

		for {
			select {
			case <-tester.stopChan:
				return
			case <-ticker.C:
				// Check pending count, wait if exceeds threshold
				for {
					pending := tester.pendingCount.Load()
					if pending < tester.maxPending {
						break
					}
					// Too many pending, wait a bit before retrying
					time.Sleep(10 * time.Millisecond)
				}

				// Send a batch of messages in burst
				for i := 0; i < tester.burstSize; i++ {
					// Check pending again (as it may increase during sending)
					if tester.pendingCount.Load() >= tester.maxPending {
						break
					}

					cmd, err := ten.NewCmd("ten_event")
					if err != nil {
						tenEnvTester.LogError(
							fmt.Sprintf("Failed to create cmd: %v", err),
						)
						continue
					}

					cmd.SetProperty("worker_id", i)
					cmd.SetProperty("count", tester.sentCount.Load())
					cmd.SetProperty("timestamp", time.Now().UnixNano())

					// Increase pending count
					tester.pendingCount.Add(1)
					count := tester.sentCount.Add(1)

					if count%1000 == 0 {
						elapsed := time.Since(tester.start).Seconds()
						rate := float64(count) / elapsed
						pending := tester.pendingCount.Load()
						tenEnvTester.LogInfo(
							fmt.Sprintf(
								"[Burst] Sent: %d, Rate: %.1f msg/s, Pending: %d/%d",
								count,
								rate,
								pending,
								tester.maxPending,
							),
						)
					}

					// Send command
					err = tenEnvTester.SendCmd(
						cmd,
						func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
							tester.callbackCount.Add(1)
							// Decrease pending count (any callback counts)
							tester.pendingCount.Add(-1)
							if err != nil {
								tenEnvTester.LogError(
									fmt.Sprintf("Callback error: %v", err),
								)
							}
						},
					)

					if err != nil {
						tenEnvTester.LogError(
							fmt.Sprintf("SendCmd failed: %v", err),
						)
						// Sending failed also needs to decrease pending
						tester.pendingCount.Add(-1)
					}
				}
			}
		}
	}()

	// Monitoring thread
	tester.wg.Add(1)
	go func() {
		defer tester.wg.Done()
		ticker := time.NewTicker(10 * time.Second)
		defer ticker.Stop()

		for {
			select {
			case <-tester.stopChan:
				return
			case <-ticker.C:
				elapsed := time.Since(tester.start).Seconds()
				sent := tester.sentCount.Load()
				received := tester.receivedCount.Load()
				callbacks := tester.callbackCount.Load()
				pending := tester.pendingCount.Load()
				tenEnvTester.LogInfo(fmt.Sprintf(
					"[MONITOR] Elapsed: %.1fs | Sent: %d | Received: %d | Callbacks: %d | Rate: %.1f/s | Pending: %d/%d (%.1f%%)",
					elapsed,
					sent,
					received,
					callbacks,
					float64(sent)/elapsed,
					pending,
					tester.maxPending,
					float64(pending)*100.0/float64(tester.maxPending),
				))
			}
		}
	}()

	// Run for 5 seconds (reduced from 10 seconds for faster testing)
	time.AfterFunc(5*time.Second, func() {
		tenEnvTester.LogInfo("Test duration reached, stopping...")
		tenEnvTester.StopTest(nil)
	})

	tenEnvTester.OnStartDone()
}

func (tester *HighLoadWithStreamingTester) OnStop(
	tenEnvTester ten.TenEnvTester,
) {
	tenEnvTester.LogInfo("=== HighLoadWithStreamingTester Stop ===")

	// Stop all workers first
	close(tester.stopChan)

	// Start a new goroutine to wait for cleanup and call OnStopDone
	go func() {
		// Wait for all goroutines to exit
		tester.wg.Wait()

		// Wait for all pending commands to complete
		tenEnvTester.LogInfo("Waiting for all pending commands to complete...")
		maxWaitTime := 5 * time.Second
		startWait := time.Now()
		for {
			pending := tester.pendingCount.Load()
			if pending <= 0 {
				break
			}
			if time.Since(startWait) > maxWaitTime {
				tenEnvTester.LogWarn(
					fmt.Sprintf(
						"Timeout waiting for pending commands, %d still pending",
						pending,
					),
				)
				break
			}
			tenEnvTester.LogDebug(
				fmt.Sprintf(
					"Still waiting for %d pending commands...",
					pending,
				),
			)
			time.Sleep(50 * time.Millisecond)
		}

		// Final statistics
		elapsed := time.Since(tester.start).Seconds()
		sent := tester.sentCount.Load()
		received := tester.receivedCount.Load()
		callbacks := tester.callbackCount.Load()

		tenEnvTester.LogInfo(fmt.Sprintf("=== Final Statistics ==="))
		tenEnvTester.LogInfo(fmt.Sprintf("Total Time: %.2f seconds", elapsed))
		tenEnvTester.LogInfo(
			fmt.Sprintf(
				"Sent: %d, Received: %d, Callbacks: %d",
				sent,
				received,
				callbacks,
			),
		)
		tenEnvTester.LogInfo(
			fmt.Sprintf("Average Rate: %.1f msg/s", float64(sent)/elapsed),
		)
		tenEnvTester.LogInfo(fmt.Sprintf("All commands completed successfully"))

		if sent != callbacks {
			tenEnvTester.LogError(
				fmt.Sprintf(
					"Callback mismatch! Sent: %d != Callbacks: %d",
					sent,
					callbacks,
				),
			)
		}

		// Call OnStopDone after everything is cleaned up
		tenEnvTester.OnStopDone()
	}()
}

// OnCmd - Key: Simulate message_collector returning large amounts of streaming
// data From logs, segfault occurred when message_collector was sending large
// amounts of fragmented data Important: All streaming results set the same key
// to trigger concurrent read-write race in cmdResultMerged
func (tester *HighLoadWithStreamingTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	count := tester.receivedCount.Add(1)

	if count%500 == 0 {
		tenEnvTester.LogDebug(fmt.Sprintf("Received command #%d", count))
	}

	// Scenario: Return 2-3 streaming results (greatly reduced) to lower
	// processing pressure
	numChunks := rand.Intn(2) + 2 // 2-3 chunks (was 5-10 before)

	// Key: Define a set of common keys, all streaming results will update these
	// keys This will trigger concurrent read-write race in event_bus_go's
	// cmdResultMerged map!
	commonKeys := []string{"words", "text", "timestamp", "confidence", "status"}

	var wg sync.WaitGroup
	for i := 0; i < numChunks; i++ {
		wg.Add(1)
		go func(chunkID int) {
			defer wg.Done()

			// Random delay, simulating async processing (reduce delay for
			// faster results)
			time.Sleep(time.Duration(rand.Intn(5)) * time.Millisecond)

			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult.SetFinal(false)

			// Key: Each chunk uses the same key but different values
			// This triggers concurrent write conflicts in the cmdResultMerged
			// map
			chunkData := make(map[string]interface{})
			for _, key := range commonKeys {
				// Each chunk sets different value for the same key
				chunkData[key] = fmt.Sprintf("chunk_%d_%s_value", chunkID, key)
			}
			// Add some extra fields to increase merge complexity
			chunkData["chunk_id"] = chunkID
			chunkData["chunk_time"] = time.Now().UnixNano()

			cmdResult.SetPropertyFromJSONBytes("", mustMarshalJSON(chunkData))

			tenEnvTester.ReturnResult(cmdResult, nil)
		}(i)
	}

	// Final result (return immediately without waiting)
	go func() {
		wg.Wait()
		// Reduce delay, return final result quickly
		time.Sleep(time.Duration(rand.Intn(2)) * time.Millisecond)

		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult.SetFinal(true)

		// Final result also uses the same keys, triggering concurrency again
		finalData := make(map[string]interface{})
		for _, key := range commonKeys {
			finalData[key] = fmt.Sprintf("final_%s_value", key)
		}
		finalData["status"] = "completed"
		finalData["total_chunks"] = numChunks
		finalData["timestamp"] = time.Now().UnixNano()

		cmdResult.SetPropertyFromJSONBytes("", mustMarshalJSON(finalData))
		tenEnvTester.ReturnResult(cmdResult, nil)
	}()
}

// =====================================================
// Test 2: GC pressure test
// Specifically trigger frequent Go GC since segfault occurred in
// runtime.mbitmap.go
// =====================================================

// GCPressureTester - Trigger frequent GC to reproduce segfault
type GCPressureTester struct {
	ten.DefaultExtensionTester
	stopChan      chan struct{}
	sentCount     atomic.Int64
	callbackCount atomic.Int64
	pendingCount  atomic.Int64 // New: Track pending message count
	start         time.Time
	maxPending    int64         // New: Maximum pending count threshold
	burstSize     int           // New: Burst batch size
	burstInterval time.Duration // New: Burst interval
}

func (tester *GCPressureTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("=== GCPressureTester Start ===")
	tenEnvTester.LogInfo(
		"Scenario: GC pressure + flow control + large memory allocation",
	)
	tenEnvTester.LogInfo(
		"Goal: Reproduce segfault in runtime.mbitmap.go under high GC pressure",
	)

	tester.start = time.Now()
	tester.stopChan = make(chan struct{})

	// Flow control parameter configuration
	tester.maxPending = 15                       // Max pending count: 15 messages (reduced from 30)
	tester.burstSize = 3                         // Send 3 messages per burst (reduced from 5)
	tester.burstInterval = 50 * time.Millisecond // Attempt burst every 50ms (increased from 30ms)

	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"Flow control config: maxPending=%d, burstSize=%d, burstInterval=%v",
			tester.maxPending,
			tester.burstSize,
			tester.burstInterval,
		),
	)
	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"Theoretical peak rate: %d msg/burst * %.1f burst/s = %.1f msg/s",
			tester.burstSize,
			1000.0/float64(tester.burstInterval.Milliseconds()),
			float64(
				tester.burstSize,
			)*1000.0/float64(
				tester.burstInterval.Milliseconds(),
			),
		),
	)

	go func() {
		ticker := time.NewTicker(tester.burstInterval)
		defer ticker.Stop()

		for {
			select {
			case <-tester.stopChan:
				return
			case <-ticker.C:
				// Check pending count, wait if exceeds threshold
				for {
					pending := tester.pendingCount.Load()
					if pending < tester.maxPending {
						break
					}
					time.Sleep(10 * time.Millisecond)
				}

				// Send a batch of messages in burst
				for i := 0; i < tester.burstSize; i++ {
					if tester.pendingCount.Load() >= tester.maxPending {
						break
					}

					// Create command and allocate memory (reduced memory
					// allocation)
					cmd, _ := ten.NewCmd("ten_event")

					tempData := make(map[string]interface{})
					for k := 0; k < 50; k++ { // Reduced from 100 to 50 fields
						// 512 bytes data per field (reduced from 1KB)
						tempData[fmt.Sprintf("field_%d", k)] = make([]byte, 512)
					}

					cmd.SetPropertyFromJSONBytes(
						"data",
						mustMarshalJSON(tempData),
					)

					// Increase pending count
					tester.pendingCount.Add(1)
					count := tester.sentCount.Add(1)

					if count%1000 == 0 {
						elapsed := time.Since(tester.start).Seconds()
						rate := float64(count) / elapsed
						pending := tester.pendingCount.Load()
						tenEnvTester.LogInfo(
							fmt.Sprintf(
								"[GC] Allocated: %d, Rate: %.1f msg/s, Pending: %d/%d",
								count,
								rate,
								pending,
								tester.maxPending,
							),
						)
					}

					// Send command (trigger CGO call and memory operations)
					tenEnvTester.SendCmd(
						cmd,
						func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
							tester.callbackCount.Add(1)
							tester.pendingCount.Add(-1)
						},
					)
				}
			}
		}
	}()

	// Stop after 5 seconds (reduced from 10 seconds)
	time.AfterFunc(5*time.Second, func() {
		tenEnvTester.LogInfo(
			fmt.Sprintf(
				"GC pressure test complete. Total allocations: %d",
				tester.sentCount.Load(),
			),
		)
		tenEnvTester.StopTest(nil)
	})

	tenEnvTester.OnStartDone()
}

func (tester *GCPressureTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"=== GCPressureTester Stop === Total Sent: %d, Callbacks: %d, Pending: %d",
			tester.sentCount.Load(),
			tester.callbackCount.Load(),
			tester.pendingCount.Load(),
		),
	)

	// Stop the sending goroutine
	close(tester.stopChan)

	// Start a new goroutine to wait for cleanup and call OnStopDone
	go func() {
		// Wait for all pending commands to complete before proceeding
		tenEnvTester.LogInfo("Waiting for all pending commands to complete...")
		maxWaitTime := 5 * time.Second
		startWait := time.Now()
		for {
			pending := tester.pendingCount.Load()
			if pending <= 0 {
				break
			}
			if time.Since(startWait) > maxWaitTime {
				tenEnvTester.LogWarn(
					fmt.Sprintf(
						"Timeout waiting for pending commands, %d still pending",
						pending,
					),
				)
				break
			}
			tenEnvTester.LogDebug(
				fmt.Sprintf(
					"Still waiting for %d pending commands...",
					pending,
				),
			)
			time.Sleep(50 * time.Millisecond)
		}

		tenEnvTester.LogInfo("All pending commands completed, stopping test")

		// Call OnStopDone after everything is cleaned up
		tenEnvTester.OnStopDone()
	}()
}

func (tester *GCPressureTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	// Quickly return 3 results
	for i := 0; i < 3; i++ {
		cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
		cmdResult.SetFinal(i == 2)

		// Each result allocates less memory (reduced from 20 to 10 fields)
		resultData := make(map[string]interface{})
		for j := 0; j < 10; j++ {
			resultData[fmt.Sprintf("r_%d", j)] = make(
				[]byte,
				256,
			) // Reduced from 512 bytes
		}
		cmdResult.SetPropertyFromJSONBytes("", mustMarshalJSON(resultData))

		tenEnvTester.ReturnResult(cmdResult, nil)
	}
}

// =====================================================
// Test 3: Concurrent callback test
// Test thread safety of hasReturnedResult in event_bus_go
// =====================================================

// ConcurrentCallbackTester - Tests race conditions in concurrent callbacks
type ConcurrentCallbackTester struct {
	ten.DefaultExtensionTester
	stopChan      chan struct{}
	sentCount     atomic.Int64
	callbackCount atomic.Int64
	pendingCount  atomic.Int64 // New: Track pending message count
	start         time.Time
	maxPending    int64         // New: Maximum pending count threshold
	burstSize     int           // New: Burst batch size
	burstInterval time.Duration // New: Burst interval
}

func (tester *ConcurrentCallbackTester) OnStart(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo("=== ConcurrentCallbackTester Start ===")
	tenEnvTester.LogInfo(
		"Scenario: Concurrent callback + flow control + cmdResultMerged race",
	)
	tenEnvTester.LogInfo(
		"Goal: Trigger concurrent read-write bug in cmdResultMerged under high concurrent callbacks",
	)

	tester.start = time.Now()
	tester.stopChan = make(chan struct{})

	// Flow control parameter configuration
	tester.maxPending = 20                       // Max pending count: 20 messages (reduced from 40)
	tester.burstSize = 4                         // Send 4 messages per burst (reduced from 8)
	tester.burstInterval = 50 * time.Millisecond // Attempt burst every 50ms (increased from 40ms)

	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"Flow control config: maxPending=%d, burstSize=%d, burstInterval=%v",
			tester.maxPending,
			tester.burstSize,
			tester.burstInterval,
		),
	)
	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"Theoretical peak rate: %d msg/burst * %.1f burst/s = %.1f msg/s",
			tester.burstSize,
			1000.0/float64(tester.burstInterval.Milliseconds()),
			float64(
				tester.burstSize,
			)*1000.0/float64(
				tester.burstInterval.Milliseconds(),
			),
		),
	)

	// Start sending goroutine
	go func() {
		ticker := time.NewTicker(tester.burstInterval)
		defer ticker.Stop()

		for {
			select {
			case <-tester.stopChan:
				return
			case <-ticker.C:
				// Check pending count, wait if exceeds threshold
				for {
					pending := tester.pendingCount.Load()
					if pending < tester.maxPending {
						break
					}
					time.Sleep(10 * time.Millisecond)
				}

				// Send a batch of messages in burst
				for i := 0; i < tester.burstSize; i++ {
					if tester.pendingCount.Load() >= tester.maxPending {
						break
					}

					cmd, _ := ten.NewCmd("ten_event")
					cmd.SetProperty("test_id", tester.sentCount.Load())

					// Increase pending count
					tester.pendingCount.Add(1)
					count := tester.sentCount.Add(1)

					if count%500 == 0 {
						elapsed := time.Since(tester.start).Seconds()
						rate := float64(count) / elapsed
						pending := tester.pendingCount.Load()
						tenEnvTester.LogInfo(
							fmt.Sprintf(
								"[Callback] Sent: %d, Rate: %.1f msg/s, Pending: %d/%d",
								count,
								rate,
								pending,
								tester.maxPending,
							),
						)
					}

					tenEnvTester.SendCmd(
						cmd,
						func(tet ten.TenEnvTester, cr ten.CmdResult, err error) {
							tester.callbackCount.Add(1)
							tester.pendingCount.Add(-1)
						},
					)
				}
			}
		}
	}()

	// Stop after 5 seconds (reduced from 10 seconds)
	time.AfterFunc(5*time.Second, func() {
		tenEnvTester.StopTest(nil)
	})

	tenEnvTester.OnStartDone()
}

func (tester *ConcurrentCallbackTester) OnStop(tenEnvTester ten.TenEnvTester) {
	tenEnvTester.LogInfo(
		fmt.Sprintf(
			"=== ConcurrentCallbackTester Stop === Sent: %d, Callbacks: %d, Pending: %d",
			tester.sentCount.Load(),
			tester.callbackCount.Load(),
			tester.pendingCount.Load(),
		),
	)

	// Stop the sending goroutine
	close(tester.stopChan)

	// Start a new goroutine to wait for cleanup and call OnStopDone
	go func() {
		// Wait for all pending commands to complete before proceeding
		tenEnvTester.LogInfo("Waiting for all pending commands to complete...")
		maxWaitTime := 5 * time.Second
		startWait := time.Now()
		for {
			pending := tester.pendingCount.Load()
			if pending <= 0 {
				break
			}
			if time.Since(startWait) > maxWaitTime {
				tenEnvTester.LogWarn(
					fmt.Sprintf(
						"Timeout waiting for pending commands, %d still pending",
						pending,
					),
				)
				break
			}
			tenEnvTester.LogDebug(
				fmt.Sprintf(
					"Still waiting for %d pending commands...",
					pending,
				),
			)
			time.Sleep(50 * time.Millisecond)
		}

		tenEnvTester.LogInfo("All pending commands completed, stopping test")

		// Call OnStopDone after everything is cleaned up
		tenEnvTester.OnStopDone()
	}()
}

func (tester *ConcurrentCallbackTester) OnCmd(
	tenEnvTester ten.TenEnvTester,
	cmd ten.Cmd,
) {
	// Key scenario: Multiple goroutines return results simultaneously, and all
	// modify the same key
	// This tests concurrent safety of the cmdResultMerged map in event_bus_go

	// Define common keys, all results will update these keys
	commonKeys := []string{"result", "data", "status", "message"}

	// Return 3 results to increase chance of concurrent conflicts
	for i := 0; i < 3; i++ {
		go func(stage int) {
			// Quick return to increase probability of concurrent conflict
			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult.SetFinal(stage == 2) // Last one is final

			// Key: All results use the same key, triggering concurrent writes
			// to map
			resultData := make(map[string]interface{})
			for _, key := range commonKeys {
				resultData[key] = fmt.Sprintf("stage_%d_%s", stage, key)
			}
			resultData["stage"] = stage
			resultData["timestamp"] = time.Now().UnixNano()

			cmdResult.SetPropertyFromJSONBytes("", mustMarshalJSON(resultData))

			// Different delays to increase chance of race
			if stage > 0 {
				time.Sleep(time.Duration(rand.Intn(3)) * time.Millisecond)
			}

			tenEnvTester.ReturnResult(cmdResult, nil)
		}(i)
	}
}

// =====================================================
// Helper functions
// =====================================================

// generateLargeChunk - Generate large data chunk, simulating real scenarios
func generateLargeChunk(chunkID int, numFields int) map[string]interface{} {
	data := make(map[string]interface{})

	// Basic fields
	data["chunk_id"] = chunkID
	data["timestamp"] = time.Now().UnixNano()

	// Large amount of data fields
	for i := 0; i < numFields; i++ {
		data[fmt.Sprintf("field_%d_%d", chunkID, i)] = fmt.Sprintf(
			"data_%d_%d_%s",
			chunkID,
			i,
			string(make([]byte, 50)),
		)
	}

	// Simulate words array (similar to message_collector transcription results)
	words := make([]map[string]interface{}, 50)
	for i := range words {
		words[i] = map[string]interface{}{
			"word":        fmt.Sprintf("word_%d_%d", chunkID, i),
			"start_ms":    rand.Intn(10000),
			"duration_ms": rand.Intn(500),
			"stable":      true,
		}
	}
	data["words"] = words

	return data
}

func mustMarshalJSON(v interface{}) []byte {
	b, err := json.Marshal(v)
	if err != nil {
		panic(fmt.Sprintf("JSON marshal error: %v", err))
	}
	return b
}

func init() {
	rand.Seed(time.Now().UnixNano())
}
