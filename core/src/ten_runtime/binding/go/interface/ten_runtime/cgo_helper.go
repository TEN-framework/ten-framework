//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"runtime"
)

type iRateLimiter interface {
	acquire()
	release()
}

type tokenBucketLimiter struct {
	tokens chan struct{}
}

func newTokenBucketLimiter(nTokens int) *tokenBucketLimiter {
	return &tokenBucketLimiter{
		tokens: make(chan struct{}, nTokens),
	}
}

func (tbl *tokenBucketLimiter) acquire() {
	tbl.tokens <- struct{}{}
}

func (tbl *tokenBucketLimiter) release() {
	<-tbl.tokens
}

// TODO(Liu): `runtime.GOMAXPROCS(0)` returns the CPUs in the host, the number
// maybe incorrect if the application runs in a docker container with cpu
// limits. Determine the actually number of CPUs in a docker container.
var defaultTokenBuckets = func() int {
	n := runtime.GOMAXPROCS(0)
	if n <= 1 {
		n = 4
	}

	return n
}()

var limiter iRateLimiter = newTokenBucketLimiter(defaultTokenBuckets)

// withCGOLimiter is used to ensure that the number of OS threads in the
// application will be limited, as new OS thread might be created due to a
// syscall such as cgo call.
//
// Why a new OS thread might be created when there is a syscall (ex: cgo call)
// in a Goroutine?
//
//  1. Once a GO application starts, it creates some idle Processor (i.e., P),
//     the number of Ps are specified by GOMAXPROCS (the number of cpu cores).
//
//  2. When a Goroutine (i.e., G) is created by the syntax `go func() {}()`, a
//     new round of go scheduler starts.
//
//  3. During each scheduler, an idle P shall be found. And then P picks one
//     Goroutine in runnable status. Each P has a runnable list of G, and also
//     there is a global runnable list of G. P will try to find the runnable G
//     from its own list first, and have a try from the global list if not
//     found. If no runnable G is found in the global neither, P will try to
//     steal from other Ps.
//
//  4. Once one runnable G is found, the P will be bound to G. And at this time,
//     an OS thread (i.e., M) is required to execute the logic of G. Each P has
//     a free list of Ms. P will try to find an idle M in its list, and create
//     a new M if not found. Then the M will be bound to G, and G will be run
//     on the M. After the G runs completed, M and P will be unbound and used
//     in the next scheduler, and M will be put into the free list in P.
//
//  5. If G makes a syscall such as calling C function, G will call
//     `entersyscall` (the entry point of any syscall from GO) to save the GO
//     stack, switch the status of G from running to "syscall blocking", and
//     unbind P from G, before calling into the C codes. In other words, the P
//     might be bound to another G after `entersyscall` (ex: another Goroutine
//     is created at this time and a new round of scheduler starts). M is still
//     bound to G now.
//
//     After `entersyscall`, the M switches to the system stack (the stack in g0
//     which is a special Goroutine created for each M, the stack in g0 is big
//     enough to make cgo call), sets up the registers and calls the C function.
//     When the C function is completed, `exitsyscall` will be called to switch
//     the status of (the original) G from "syscall blocking" to runnable,
//     unbind M from G, and start a new round of scheduler.
//
//     Note: Theoretically, at this stage, the Goroutine (G) does not need to
//     unbind from the Machine (M). The Go runtime does this presumably for the
//     sake of scheduling fairness, to trigger a new scheduling. This practice
//     is also common in other context; for example, in operating systems,
//     returning from a system call is a scheduling point.
//
//     In summary, during calling from GO to C, the P is unbound before calling
//     C function, and the C function runs on the same M.
//
//     Therefore, if the Processor (P) is idle, and there is a Goroutine (G)
//     that is runnable but not yet bound to a P, then that G may be bound to
//     this idle P. At this time, a new M will be created as the previous one is
//     used to execute the C codes now.
//
// So we can say, the maximum of M = the number of concurrent syscall +
// GOMAXPROCS. If we want to limit the maximum of M, we have to limit the number
// of concurrent syscall. That's what withCGOLimiter does.

// Essentially, the functions `withCGOLimiter` and
// `withCGOLimiterHasReturnValue` serve the same purpose, acting as rate
// limiters for cgo calls. The difference between the two functions lies in
// their return types:
//
// - withCGOLimiter only returns a possible error
// - while withCGOLimiterHasReturnValue returns both a return value and a
//   possible error.
//
// Although it is possible to use withCGOLimiter to pass out a return value
// using the method below, this approach involves heap allocation.
//
//   func func1() {
//		  var (
//			  result T
//			  err error
//		  )
//
//		  _ = withCGOLimiter(func() error {
//			  result, err = fn()
//		  }
//	 }
//
// The `result` and `err` will be allocated on the heap, but not on the stack,
// as they are captured by the closure. That's heap escapes in GO.
//
// Using the `withCGOLimiterHasReturnValue` can avoid the heap escapes.

// withCGOLimiter executes the function surrounded with a rate limiter. The fn
// will be blocked if the rate limit has exceeded, otherwise fn will be executed
// directly.
//
// !IMPORTANT!
// `withCGOLimiter(fn)` is used to limit the number of concurrent executions of
// `fn`. The reason for this is that `fn` typically contains calls to cgo, and
// if the concurrency of these cgo calls isn't controlled, it can lead to the
// creation of a large number of pthreads. This may cause excessive use of
// system resources and even system crashes.
//
// Because of this, **there must be no blocking behavior inside the `fn` passed
// to `withCGOLimiter(fn)`**. For example, blocking operations like waiting on a
// channel should be avoided. This is due to the current Go binding
// architecture, where `on_xxx` callbacks run on the extension thread. If
// `withCGOLimiter(fn)` is invoked inside an `on_xxx` callback, it might end up
// blocking the extension thread (since the limit on concurrent
// `withCGOLimiter(fn)` calls is reached and it has to wait for a previous `fn`
// to finish). However, that earlier `fn` might also be stuck waiting —
// usually on the extension thread to complete the second half of some async
// task — resulting in a **deadlock**.
//
// To put it simply, `withCGOLimiter(fn)` is necessary to limit the number of
// concurrent cgo calls. But we **must not** perform any blocking operations
// inside the `fn` passed to it.
func withCGOLimiter(fn func() error) error {
	limiter.acquire()
	defer limiter.release()

	err := fn()
	return err
}

// Executes the function surrounded with a rate limiter. The fn will be blocked
// if the rate limit has exceeded, otherwise fn will be executed directly.
func withCGOLimiterHasReturnValue[T any](fn func() (T, error)) (T, error) {
	limiter.acquire()
	defer limiter.release()

	return fn()
}
