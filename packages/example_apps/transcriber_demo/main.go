//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package main

import (
	"flag"
	"fmt"
	"os"

	ten "ten_framework/ten_runtime"

	"github.com/joho/godotenv"
)

type appConfig struct {
	PropertyFilePath string
}

type defaultApp struct {
	ten.DefaultApp

	cfg *appConfig
}

func (p *defaultApp) OnConfigure(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("onConfigure")

	// Load .env file
	err := godotenv.Load()
	if err != nil {
		tenEnv.LogWarn("Warning: .env file not found, continuing without it")
	} else {
		tenEnv.LogInfo("Successfully loaded .env file")
	}

	// Using the default property.json if not specified.
	if len(p.cfg.PropertyFilePath) > 0 {
		if b, err := os.ReadFile(p.cfg.PropertyFilePath); err != nil {
			tenEnv.LogError(
				fmt.Sprintf(
					"Failed to read property file %s, err %v\n",
					p.cfg.PropertyFilePath,
					err,
				),
			)
		} else {
			tenEnv.InitPropertyFromJSONBytes(b)
		}
	}

	tenEnv.OnConfigureDone()
}

func (p *defaultApp) OnInit(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("onInit")
	tenEnv.OnInitDone()
}

func (p *defaultApp) OnDeinit(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("onDeinit")
	tenEnv.OnDeinitDone()
}

func startAppBlocking(cfg *appConfig) {
	appInstance, err := ten.NewApp(&defaultApp{
		cfg: cfg,
	})
	if err != nil {
		fmt.Printf("Failed to create the app, %v\n", err)
		os.Exit(1)
	}

	appInstance.Run(true)
	appInstance.Wait()

	ten.EnsureCleanupWhenProcessExit()
}

func main() {
	cfg := &appConfig{}

	flag.StringVar(
		&cfg.PropertyFilePath,
		"property",
		"",
		"The absolute path of property.json",
	)
	flag.Parse()

	startAppBlocking(cfg)
}
