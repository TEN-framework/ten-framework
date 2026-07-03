//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"encoding/json"
	"fmt"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"

	ten "ten_framework/ten_runtime"
)

const concurrency = 100

type extensionA struct {
	ten.DefaultExtension
}

func newExtensionA(name string) ten.Extension {
	return &extensionA{}
}

// makeJSONOfSize creates a JSON object whose serialized form is exactly `size`
// bytes (the strlen, without '\0'). This targets the pool boundary sizes
// (128, 512, 1024, 2048, 4096) so that acquireBytes returns a buffer whose
// cap == size, leaving zero room for the extra '\0' that strcpy writes.
func makeJSONOfSize(targetSize int) []byte {
	// {"k":"<padding>"} is 9 bytes of overhead, so padding = targetSize - 9
	padLen := targetSize - 9
	if padLen < 0 {
		padLen = 0
	}
	obj := map[string]string{"k": strings.Repeat("x", padLen)}
	b, _ := json.Marshal(obj)
	// Fine-tune: if json.Marshal escaping changed the length, just return as-is.
	return b
}

func (p *extensionA) OnStart(tenEnv ten.TenEnv) {
	go func() {
		// ---------------------------------------------------------------
		// Repro: stress GetPropertyToJSONBytes at exact pool boundary sizes.
		//
		// The bug: ten_go_ten_c_value_to_json sets json_str_len = strlen(json_str)
		// (excludes '\0'), but ten_go_copy_c_str_to_slice_and_free uses strcpy
		// which writes strlen+1 bytes. Go allocates exactly strlen bytes from
		// the pool, so strcpy overflows by 1 byte into the next heap object.
		//
		// When the JSON size equals a pool boundary (128, 512, 1024, 2048, 4096),
		// acquireBytes returns a slice with cap == boundary == strlen, and the
		// overflow corrupts the adjacent heap object. Concurrent GC then crashes
		// with SIGSEGV at address 0xffffffffffffffff when scanning corrupted
		// pointers.
		// ---------------------------------------------------------------

		// Pool boundary sizes from bytes.go
		boundaries := []int{128, 512, 1024, 2048, 4096}

		const iterations = 100
		var wg sync.WaitGroup
		var errCount int32

		for _, bSize := range boundaries {
			for i := 0; i < iterations; i++ {
				wg.Add(1)
				go func(size, idx int) {
					defer wg.Done()
					defer func() {
						if r := recover(); r != nil {
							atomic.AddInt32(&errCount, 1)
							fmt.Printf("PANIC at size=%d idx=%d: %v\n", size, idx, r)
						}
					}()

					propName := fmt.Sprintf("json_%d_%d", size, idx)
					payload := makeJSONOfSize(size)

					// Set JSON property
					if err := tenEnv.SetPropertyFromJSONBytes(propName, payload); err != nil {
						fmt.Printf("SetPropertyFromJSONBytes error: %v\n", err)
						return
					}

					// Read it back — this triggers the strcpy overflow
					got, err := tenEnv.GetPropertyToJSONBytes(propName)
					if err != nil {
						fmt.Printf("GetPropertyToJSONBytes error: %v\n", err)
						return
					}

					// Unmarshal + re-marshal (mimics enhanceTextData)
					var m map[string]interface{}
					if err := json.Unmarshal(got, &m); err != nil {
						fmt.Printf("Unmarshal error at size=%d: %v\n", size, err)
						atomic.AddInt32(&errCount, 1)
						return
					}
					m["extra"] = idx
					if _, err := json.Marshal(m); err != nil {
						fmt.Printf("Marshal error at size=%d: %v\n", size, err)
						atomic.AddInt32(&errCount, 1)
						return
					}

					// Force GC to scan heap — corrupted pointers trigger SIGSEGV
					if idx%20 == 0 {
						runtime.GC()
					}
				}(bSize, i)
			}
		}

		wg.Wait()

		if errCount > 0 {
			fmt.Printf("ERROR: %d failures detected (heap corruption from strcpy overflow)\n", errCount)
		} else {
			fmt.Println("All boundary-size JSON round-trips passed")
		}

		tenEnv.OnStartDone()
	}()
}

func (p *extensionA) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		fmt.Println("extensionA OnCmd")

		cmdB, _ := ten.NewCmd("B")
		var count uint32 = 0
		var propLock sync.Mutex

		done := make(chan struct{}, 1)
		defer close(done)

		for i := 0; i < concurrency; i++ {
			go func(i int) {
				propLock.Lock()
				err := cmdB.SetProperty(fmt.Sprintf("prop_%d", i), i)
				propLock.Unlock()

				if err != nil {
					panic("should not happen")
				}

				cmdName, _ := cmdB.GetName()
				if cmdName != "B" {
					panic("should not happen")
				}

				if atomic.AddUint32(&count, 1) == concurrency {
					done <- struct{}{}
				}
			}(i % 100)
		}
		<-done

		if err := cmdB.SetPropertyString("empty_string", ""); err != nil {
			panic("Should not happen.")
		}
		if err := cmdB.SetPropertyBytes("empty_bytes", []byte{}); err == nil {
			panic("Should not happen.")
		}
		some_bytes := []byte{1, 2, 3}
		if err := cmdB.SetPropertyBytes("some_bytes", some_bytes); err != nil {
			panic("Should not happen.")
		}

		tenEnv.SendCmd(cmdB, func(r ten.TenEnv, cs ten.CmdResult, e error) {
			detail, err := cs.GetPropertyString("detail")
			if err != nil {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
				cmdResult.SetPropertyString("detail", err.Error())
				r.ReturnResult(cmdResult, nil)
				return
			}

			if detail != "this is extensionB." {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
				cmdResult.SetPropertyString("detail", "wrong detail")
				r.ReturnResult(cmdResult, nil)
				return
			}

			password, err := cs.GetPropertyString("password")
			if err != nil {
				cmdResult, _ := ten.NewCmdResult(ten.StatusCodeError, cmd)
				cmdResult.SetPropertyString("detail", err.Error())
				r.ReturnResult(cmdResult, nil)
				return
			}

			cmdResult, _ := ten.NewCmdResult(ten.StatusCodeOk, cmd)
			cmdResult.SetPropertyString("detail", password)
			r.ReturnResult(cmdResult, nil)
		})
	}()
}

func init() {
	err := ten.RegisterAddonAsExtension(
		"extension_a",
		ten.NewDefaultExtensionAddon(newExtensionA),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
