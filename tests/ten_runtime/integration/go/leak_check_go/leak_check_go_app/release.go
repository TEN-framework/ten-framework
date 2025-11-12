//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

//go:build !debug
// +build !debug

package main

// void exit(int status);
import "C"

import (
	"runtime"
	"runtime/debug"
	"time"
)

func CleanUpAndCheckMemoryLeak() {
	// A single GC is not enough; multiple rounds of GC are needed to clean up
	// as thoroughly as possible.
	for i := 0; i < 10; i++ {
		// Explicitly trigger GC to increase the likelihood of finalizer
		// execution.
		debug.FreeOSMemory()
		runtime.GC()

		// Wait for a short period to give the GC time to run.
		runtime.Gosched()
		time.Sleep(1 * time.Second)
	}

	C.exit(0)
}
