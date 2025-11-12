//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package tests

import (
	"fmt"
	"os"
	ten "ten_framework/ten_runtime"
	"testing"

	// We import the default_extension_go package to ensure its init function is
	// executed. This is because default_extension_go registers the addon in its
	// init function.
	// Note that this line is necessary, even though it doesn't seem to be used.
	_ "default_extension_go"
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
									"level": "debug"
								}
							],
							"formatter": {
								"type": "plain",
								"colored": true
							},
							"emitter": {
								"type": "console",
								"config": {
									"stream": "stdout"
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

	LeakCheck()
}

func TestMain(m *testing.M) {
	setup()
	code := m.Run()
	teardown()
	os.Exit(code)
}
