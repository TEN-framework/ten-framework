//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package tests

import (
	"fmt"
	"os"
	"runtime"
	"runtime/debug"
	ten "ten_framework/ten_runtime"
	"testing"
	"time"

	// We import the event_bus_go package to ensure its init function is
	// executed. This is because event_bus_go registers the addon in its
	// init function.
	// Note that this line is necessary, even though it doesn't seem to be used.
	_ "event_bus_go"
)

var globalApp ten.App

type fakeApp struct {
	ten.DefaultApp

	initDoneChan chan bool
}

func (p *fakeApp) OnConfigure(tenEnv ten.TenEnv) {
	// Default log config
	tenEnv.InitPropertyFromJSONBytes(
		[]byte(`{
			"ten": {
				"log": {
					"handlers": [
						{
							"matchers": [
								{
									"level": "info"
								}
							],
							"formatter": {
								"type": "plain",
								"colored": false
							},
							"emitter": {
								"type": "console",
								"config": {
									"stream": "stderr"
								}
							}
						}
					]
				}
			}
		}`),
	)

	tenEnv.OnConfigureDone()
}

func (p *fakeApp) OnInit(tenEnv ten.TenEnv) {
	tenEnv.OnInitDone()
	p.initDoneChan <- true
}

func setup() {
	fakeApp := &fakeApp{
		initDoneChan: make(chan bool),
	}

	var err error

	globalApp, err = ten.NewApp(fakeApp)
	if err != nil {
		fmt.Println("Failed to create global app.")
	}

	globalApp.Run(true)

	<-fakeApp.initDoneChan
}

func teardown() {
	globalApp.Close()
	globalApp.Wait()

	globalApp = nil
}

func gcAndFreeOSMemory() {
	for i := 0; i < 10; i++ {
		// Explicitly trigger GC to increase the likelihood of finalizer
		// execution.
		debug.FreeOSMemory()
		runtime.GC()

		// Wait for a short period to give the GC time to run.
		runtime.Gosched()
		time.Sleep(1 * time.Second)
	}
}

func TestMain(m *testing.M) {
	setup()
	code := m.Run()
	teardown()

	gcAndFreeOSMemory()
	os.Exit(code)
}
