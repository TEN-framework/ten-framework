//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

//go:build debug
// +build debug

package main

// void __lsan_do_leak_check(void);
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

	// To detect memory leaks with ASan, need to enable the following cgo code.
	C.__lsan_do_leak_check()

	// According to
	// https://github.com/golang/go/issues/20713#issuecomment-1518197679, if
	// asan is enabled, it means that -race is not specified in go build, so the
	// destructor of the ten_runtime may not be executed, so we need to call the
	// exit method manually.
	C.exit(0)
}
