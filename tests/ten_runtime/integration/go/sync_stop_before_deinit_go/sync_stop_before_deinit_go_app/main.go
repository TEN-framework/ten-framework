//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package main

import (
	"fmt"
	"runtime"
	"runtime/debug"
	"time"

	ten "ten_framework/ten_runtime"
)

type defaultApp struct {
	ten.DefaultApp
}

func appRoutine(app ten.App, stopped chan<- struct{}) {
	app.Run(false)
	stopped <- struct{}{}
}

func main() {
	// test app
	app, err := ten.NewApp(&defaultApp{})
	if err != nil {
		fmt.Println("Failed to create app.")
	}

	stopped := make(chan struct{}, 1)
	go appRoutine(app, stopped)
	<-stopped

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
		time.Sleep(1 * time.Second)
	}
}
