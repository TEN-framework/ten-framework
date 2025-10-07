/**
 *
 * Agora Real Time Engagement
 * Created by Wei Hu in 2022-10.
 * Copyright (c) 2024 Agora IO. All rights reserved.
 *
 */
package main

import (
	"flag"
	"log"
	"os"

	ten "ten_framework/ten_runtime"
)

type appConfig struct {
	PropertyFilePath string
	TenappDir        string
}

type defaultApp struct {
	ten.DefaultApp

	cfg *appConfig
}

func (p *defaultApp) OnConfigure(
	tenEnv ten.TenEnv,
) {
	// Change working directory if tenapp_dir is specified
	if len(p.cfg.TenappDir) > 0 {
		if err := os.Chdir(p.cfg.TenappDir); err != nil {
			log.Fatalf("Failed to change working directory to %s, err %v\n", p.cfg.TenappDir, err)
		}
		log.Printf("Changed working directory to: %s\n", p.cfg.TenappDir)
	}

	// Using the default property.json if not specified.
	if len(p.cfg.PropertyFilePath) > 0 {
		if b, err := os.ReadFile(p.cfg.PropertyFilePath); err != nil {
			log.Fatalf("Failed to read property file %s, err %v\n", p.cfg.PropertyFilePath, err)
		} else {
			tenEnv.InitPropertyFromJSONBytes(b)
		}
	}

	tenEnv.OnConfigureDone()
}

func startAppBlocking(cfg *appConfig) {
	appInstance, err := ten.NewApp(&defaultApp{
		cfg: cfg,
	})
	if err != nil {
		log.Fatalf("Failed to create the app, %v\n", err)
	}

	appInstance.Run(true)
	appInstance.Wait()

	ten.EnsureCleanupWhenProcessExit()
}

func setDefaultLog() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)
}

func main() {
	// Set the default log format globally, users can use `log.Println()` directly.
	setDefaultLog()

	cfg := &appConfig{}

	flag.StringVar(&cfg.PropertyFilePath, "property", "", "The absolute path of property.json")
	flag.StringVar(&cfg.TenappDir, "tenapp_dir", "", "The base folder path for tman run start command")
	flag.Parse()

	startAppBlocking(cfg)
}
