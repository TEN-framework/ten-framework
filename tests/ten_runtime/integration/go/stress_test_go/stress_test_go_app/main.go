//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package main

import (
	"fmt"
	"os"
	"runtime"
	"runtime/debug"
	"time"

	ten "ten_framework/ten_runtime"
)

type defaultApp struct {
	ten.DefaultApp
}

func (p *defaultApp) OnConfigure(tenEnv ten.TenEnv) {
	tenEnv.LogInfo("=== TEN GC Stress Test App ===")

	// 打印 GC 配置信息
	gcPercent := debug.SetGCPercent(-1)
	debug.SetGCPercent(gcPercent)

	gogcEnv := os.Getenv("GOGC")
	if gogcEnv == "" {
		gogcEnv = "100 (default)"
	}

	tenEnv.LogInfo(fmt.Sprintf("GOGC: %s (actual: %d%%)", gogcEnv, gcPercent))
	tenEnv.LogInfo(fmt.Sprintf("GOMAXPROCS: %d", runtime.GOMAXPROCS(0)))
	tenEnv.LogInfo(fmt.Sprintf("NumCPU: %d", runtime.NumCPU()))
	tenEnv.LogInfo(fmt.Sprintf("Go Version: %s", runtime.Version()))

	godebug := os.Getenv("GODEBUG")
	if godebug != "" {
		tenEnv.LogInfo(fmt.Sprintf("GODEBUG: %s", godebug))
	}

	tenEnv.LogInfo("================================")

	tenEnv.OnConfigureDone()
}

func (p *defaultApp) OnInit(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("onInit")
	tenEnv.OnInitDone()
}

func (p *defaultApp) OnDeinit(tenEnv ten.TenEnv) {
	tenEnv.LogDebug("onDeinit")

	// 打印最终的内存统计
	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	tenEnv.LogInfo("=== Final Memory Statistics ===")
	tenEnv.LogInfo(fmt.Sprintf("Heap Alloc: %.2f MB", float64(m.Alloc)/1024/1024))
	tenEnv.LogInfo(fmt.Sprintf("Total Alloc: %.2f MB", float64(m.TotalAlloc)/1024/1024))
	tenEnv.LogInfo(fmt.Sprintf("Sys: %.2f MB", float64(m.Sys)/1024/1024))
	tenEnv.LogInfo(fmt.Sprintf("GC Runs: %d", m.NumGC))
	tenEnv.LogInfo(fmt.Sprintf("GC Pause Total: %v", time.Duration(m.PauseTotalNs)))
	if m.NumGC > 0 {
		tenEnv.LogInfo(fmt.Sprintf("GC Pause Avg: %v", time.Duration(m.PauseTotalNs)/time.Duration(m.NumGC)))
	}
	tenEnv.LogInfo("================================")

	tenEnv.OnDeinitDone()
}

func main() {
	// 打印启动信息
	fmt.Printf("=== TEN GC Stress Test Starting ===\n")
	fmt.Printf("GOGC: %s\n", getEnvWithDefault("GOGC", "100 (default)"))
	fmt.Printf("GODEBUG: %s\n", getEnvWithDefault("GODEBUG", "(none)"))
	fmt.Printf("Go Version: %s\n", runtime.Version())
	fmt.Printf("NumCPU: %d\n", runtime.NumCPU())
	fmt.Printf("GOMAXPROCS: %d\n", runtime.GOMAXPROCS(0))
	fmt.Printf("====================================\n\n")

	// test app
	app, err := ten.NewApp(&defaultApp{})
	if err != nil {
		fmt.Println("Failed to create app.")
		os.Exit(1)
	}

	app.Run(true)
	app.Wait()

	fmt.Println("\n=== App stopped, running cleanup GC ===")

	// A single GC is not enough; multiple rounds of GC are needed to clean up
	// as thoroughly as possible.
	//
	// Note: Because the ten-runtime's own leak check mechanism is enabled
	// during testing, we still need the following multiple GC calls to actually
	// trigger the Go layer's finalizers to avoid a situation where Go
	// finalizers are not called before the entire process exits, which would
	// cause what could be considered a real memory leak to some extent.
	for i := 0; i < 10; i++ {
		// Explicitly trigger GC to increase the likelihood of finalizer
		// execution.
		debug.FreeOSMemory()
		runtime.GC()

		// Wait for a short period to give the GC time to run.
		runtime.Gosched()
		time.Sleep(100 * time.Millisecond)

		// 每隔几轮打印一次内存状态
		if i%3 == 0 {
			var m runtime.MemStats
			runtime.ReadMemStats(&m)
			fmt.Printf("Cleanup round %d: HeapAlloc=%.2fMB NumGC=%d\n",
				i+1,
				float64(m.HeapAlloc)/1024/1024,
				m.NumGC,
			)
		}
	}

	fmt.Println("=== GC Stress Test Completed ===")
}

func getEnvWithDefault(key, defaultValue string) string {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue
	}
	return value
}
