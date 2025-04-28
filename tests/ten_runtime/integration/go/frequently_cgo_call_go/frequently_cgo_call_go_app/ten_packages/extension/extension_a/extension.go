//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package default_extension_go

import (
	"bytes"
	"encoding/json"
	"fmt"
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

func (p *extensionA) OnStart(tenEnv ten.TenEnv) {
	go func() {
		var wg sync.WaitGroup
		var counter int32

		wg.Add(concurrency)

		for i := int64(0); i < concurrency; i++ {
			go func(i int64) {
				defer wg.Done()

				err := tenEnv.SetProperty(
					fmt.Sprintf("prop_%d", i),
					i,
				)
				if err != nil {
					fmt.Printf("Error in goroutine %d: %v\n", i, err)
					panic("Should not happen")
				}

				read_back_value, err := tenEnv.GetPropertyInt64(
					fmt.Sprintf("prop_%d", i),
				)
				if err != nil {
					fmt.Printf("Error in goroutine %d: %v\n", i, err)
					panic("Should not happen")
				}

				if read_back_value != i {
					fmt.Printf(
						"Error in goroutine %d: %v\n",
						i,
						read_back_value,
					)
					panic("Should not happen")
				}

				if atomic.AddInt32(&counter, 1)%10 == 0 {
					fmt.Printf("extension_a %d goroutines completed\n", counter)
				}
			}(i % 100)
		}

		wg.Wait()
		tenEnv.OnStartDone()
	}()

	for i := int64(0); i < 100; i++ {
		if err := tenEnv.SetProperty(fmt.Sprintf("prop_bool_%d", i), true); err != nil {
			panic("Should not happen")
		}

		if em, err := tenEnv.GetPropertyBool(fmt.Sprintf("prop_bool_%d", i)); err != nil ||
			em != true {
			fmt.Printf(
				"Error %v\n",
				em,
			)
			panic("Should not happen")
		}

		if i%10 == 0 {
			fmt.Printf("set prop_bool_%d\n and get check passed\n", i)
		}

		if err := tenEnv.SetProperty(fmt.Sprintf("prop_int_%d", i), int64(i)); err != nil {
			panic("Should not happen")
		}

		if em, err := tenEnv.GetPropertyInt64(fmt.Sprintf("prop_int_%d", i)); err != nil ||
			em != int64(i) {
			fmt.Printf(
				"Error %v\n",
				em,
			)
			panic("Should not happen")
		}

		if i%10 == 0 {
			fmt.Printf("set prop_int_%d\n and get check passed\n", i)
		}

		if err := tenEnv.SetPropertyString(fmt.Sprintf("prop_string_%d", i), fmt.Sprintf("string_%d", i)); err != nil {
			panic("Should not happen")
		}

		if em, err := tenEnv.GetPropertyString(fmt.Sprintf("prop_string_%d", i)); err != nil ||
			em != fmt.Sprintf("string_%d", i) {
			fmt.Printf(
				"Error %v\n",
				em,
			)
			panic("Should not happen")
		}

		if i%10 == 0 {
			fmt.Printf("set prop_string_%d\n and get check passed\n", i)
		}

		if err := tenEnv.SetPropertyBytes(fmt.Sprintf("prop_bytes_%d", i), []byte(fmt.Sprintf("bytes_%d", i))); err != nil {
			panic("Should not happen")
		}

		em, err := tenEnv.GetPropertyBytes(fmt.Sprintf("prop_bytes_%d", i))
		if err != nil {
			fmt.Printf("Error in goroutine %d: %v\n", i, err)
			panic("Should not happen")
		}

		// compare the bytes
		if !bytes.Equal(em, []byte(fmt.Sprintf("bytes_%d", i))) {
			fmt.Printf("Error in goroutine %d: %v\n", i, em)
			panic("Should not happen")
		}

		if i%10 == 0 {
			fmt.Printf("set prop_bytes_%d\n and get check passed\n", i)
		}

		if err := tenEnv.SetPropertyFromJSONBytes(fmt.Sprintf("prop_json_%d", i), []byte(fmt.Sprintf("{\"key\":\"value_%d\"}", i))); err != nil {
			panic("Should not happen")
		}

		// Parse the JSON bytes to a map[string]interface{}
		jsonBytes, err := tenEnv.GetPropertyToJSONBytes(fmt.Sprintf("prop_json_%d", i))
		if err != nil {
			panic("Should not happen")
		}

		var jsonMap map[string]interface{}
		err = json.Unmarshal(jsonBytes, &jsonMap)
		if err != nil {
			panic("Should not happen")
		}

		if jsonMap["key"] != fmt.Sprintf("value_%d", i) {
			fmt.Printf(
				"Error %v\n",
				jsonMap,
			)
			panic("Should not happen")
		}

		if i%10 == 0 {
			fmt.Printf("set prop_json_%d\n and get check passed\n", i)
		}
	}
}

func (p *extensionA) OnCmd(
	tenEnv ten.TenEnv,
	cmd ten.Cmd,
) {
	go func() {
		fmt.Println("extensionA OnCmd")

		var propLock sync.Mutex

		cmdB, _ := ten.NewCmd("B")
		var count uint32 = 0

		done := make(chan struct{}, 1)
		defer close(done)

		for i := 0; i < concurrency; i++ {
			go func(i int) {
				propLock.Lock()

				// The SetProperty is not goroutine safety.
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

		// Set an empty string to cmd is permitted.
		if err := cmdB.SetPropertyString("empty_string", ""); err != nil {
			panic("Should not happen.")
		}

		// Set an empty bytes to cmd is not permitted.
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
	// Register addon
	err := ten.RegisterAddonAsExtension(
		"extension_a",
		ten.NewDefaultExtensionAddon(newExtensionA),
	)
	if err != nil {
		fmt.Println("register addon failed", err)
	}
}
