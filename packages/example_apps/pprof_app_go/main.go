//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//

package main

import (
	"fmt"
	"net/http"
	"os"
	"time"

	"runtime/pprof"

	ten "ten_framework/ten_runtime"
)

type defaultApp struct {
	ten.DefaultApp

	stop chan struct{}
}

func new() *defaultApp {
	return &defaultApp{
		stop: make(chan struct{}, 1),
	}
}

func (p *defaultApp) OnInit(
	tenEnv ten.TenEnv,
) {
	fmt.Println("Pprof app onInit")

	tenEnv.OnInitDone()

	go func(stop chan struct{}) {
		rtePprofServerPort := os.Getenv("TEN_PROFILER_SERVER_PORT")
		if rtePprofServerPort != "" {
			fmt.Println("TEN_PROFILER_SERVER_PORT:", rtePprofServerPort)
			go http.ListenAndServe(fmt.Sprintf(":%s", rtePprofServerPort), nil)
		}

		heapDumpDir := os.Getenv("TEN_HEAP_DUMP_DIR")
		heapTimeInterval := os.Getenv("HEAP_PROFILE_TIME_INTERVAL")

		if heapDumpDir != "" && heapTimeInterval != "" {
			// convert string to int
			interval := 0
			fmt.Sscanf(heapTimeInterval, "%d", &interval)
			if interval <= 0 {
				fmt.Println("heapTimeInterval invalid.")
				return
			}

			fmt.Println("heapDumpDir:", heapDumpDir)
			fmt.Println("heapTimeInterval:", interval)

			if os.MkdirAll(heapDumpDir+"/go", 0755) != nil {
				fmt.Println("Failed to create heapDumpDir")
			}

			heapDumpDir = heapDumpDir + "/go"

			if err := dumpHeap(heapDumpDir); err != nil {
				fmt.Println("dumpHeap failed:", err)
			}

			for {
				t := time.NewTicker(time.Duration(interval) * time.Second)

				select {
				case <-stop:
					return
				case <-t.C:
					if err := dumpHeap(heapDumpDir); err != nil {
						fmt.Println("dumpHeap failed:", err)
					}
				}

			}
		}
	}(p.stop)
}

func dumpHeap(heapDumpDir string) error {
	timestamp := time.Now().Unix()
	heapFile, err := os.OpenFile(
		fmt.Sprintf(heapDumpDir+"/heap_%d.out", timestamp),
		os.O_CREATE|os.O_RDWR,
		0644,
	)
	if err == nil {
		pprof.WriteHeapProfile(heapFile)
		heapFile.Close()
	}

	return err
}

func (p *defaultApp) OnDeinit(tenEnv ten.TenEnv) {
	fmt.Println("DefaultApp onDeinit")

	p.stop <- struct{}{}

	tenEnv.OnDeinitDone()
}

func main() {
	// test app
	appInstance, err := ten.NewApp(new())
	if err != nil {
		fmt.Println("Failed to create app.")
	}

	appInstance.Run(true)
	appInstance.Wait()
}
