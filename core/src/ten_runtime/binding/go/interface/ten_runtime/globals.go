//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

const (
	intSize = 32 << (^uint(0) >> 63) // 32 or 64
	is64bit = intSize == 64
)

// @{
// Internal use only.

// EnsureCleanupWhenProcessExit is essentially a self-check mechanism, not a
// publicly exposed interface of the TEN runtime. Inside this function, it can
// be used to check whether some resources are in a cleared state. If they are
// not in a cleared state, it means we have made a mistake.
//
// As the golang runtime does not provide a behavior to release the global
// variables (as the global variable is not recommended in golang), what we can
// do is to explicitly call this function when the TEN app "process" is closed.
//
// Some objects in golang runtime can be used as global variables, such as
// sync.Pool, as it registers a shutdown hook (i.e., runtime_registerPoolCleanup
// in pool.go) which will be invoked before each GC cycle. However,
// `runtime_registerPoolCleanup` is an internal api in golang runtime, there is
// no exported apis for us.
func EnsureCleanupWhenProcessExit() {
	// TODO(Liu): check if the handle map is empty.
}

// @}
