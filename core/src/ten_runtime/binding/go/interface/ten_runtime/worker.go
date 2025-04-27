//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

import (
	"errors"
	"sync"
	"sync/atomic"
)

var (
	// ErrPoolNotRunning is returned when the pool is not running.
	ErrPoolNotRunning = errors.New("the pool is not running")

	// ErrWorkerClosed is returned when the worker is closed.
	ErrWorkerClosed = errors.New("worker was closed")
)

// The global pool is used to execute cgo functions without creating two many
// native threads.
var globalPool pool = *newJobPool(10)

type job func() any

type worker interface {
	process(any) any
	terminate()
}

//------------------------------------------------------------------------------

type closureWorker struct {
	processor func(any) any
}

var _ worker = new(closureWorker)

func (p *closureWorker) process(payload any) any {
	return p.processor(payload)
}

func (p *closureWorker) terminate() {}

//------------------------------------------------------------------------------

type workRequest struct {
	jobChan chan<- any
	resChan <-chan any
}

//------------------------------------------------------------------------------

type workerWrapper struct {
	worker
	reqChan    chan<- workRequest
	closeChan  chan struct{}
	closedChan chan struct{}
}

func newWorkerWrapper(reqChan chan<- workRequest, w worker) *workerWrapper {
	wrapper := &workerWrapper{
		worker:     w,
		reqChan:    reqChan,
		closeChan:  make(chan struct{}),
		closedChan: make(chan struct{}),
	}

	go wrapper.run()

	return wrapper
}

func (p *workerWrapper) run() {
	jobChan, resChan := make(chan any), make(chan any)
	defer func() {
		p.worker.terminate()
		close(resChan)
		close(p.closedChan)
	}()

	for {
		select {
		case p.reqChan <- workRequest{
			jobChan: jobChan,
			resChan: resChan,
		}:
			payload := <-jobChan
			result := p.process(payload)
			resChan <- result
		case <-p.closeChan:
			return
		}
	}
}

func (p *workerWrapper) stop() {
	close(p.closeChan)
}

func (p *workerWrapper) join() {
	<-p.closedChan
}

//------------------------------------------------------------------------------

type pool struct {
	queuedJobsCount int64

	ctor    func() worker
	workers []*workerWrapper
	reqChan chan workRequest

	workerMut sync.Mutex
}

func newPool(n int, ctor func() worker) *pool {
	p := &pool{
		ctor:    ctor,
		reqChan: make(chan workRequest),
	}
	p.setSize(n)
	return p
}

func newFuncPool(n int, f func(any) any) *pool {
	return newPool(n, func() worker {
		return &closureWorker{
			processor: f,
		}
	})
}

func newJobPool(n int) *pool {
	return newFuncPool(n, func(a any) any {
		f, ok := a.(job)
		if !ok {
			return newTenError(
				ErrorCodeGeneric,
				"job is not a function.",
			)
		}

		return f()
	})
}

func (p *pool) process(payload job) any {
	atomic.AddInt64(&p.queuedJobsCount, 1)

	request, open := <-p.reqChan
	if !open {
		return ErrPoolNotRunning
	}

	request.jobChan <- payload

	res, open := <-request.resChan
	if !open {
		return ErrWorkerClosed
	}

	atomic.AddInt64(&p.queuedJobsCount, -1)
	return res
}

func (p *pool) queueLength() int64 {
	return atomic.LoadInt64(&p.queuedJobsCount)
}

func (p *pool) setSize(n int) {
	p.workerMut.Lock()
	defer p.workerMut.Unlock()

	lWorkers := len(p.workers)
	if lWorkers == n {
		return
	}

	// Add extra workers if N > len(workers)
	for i := lWorkers; i < n; i++ {
		p.workers = append(p.workers, newWorkerWrapper(p.reqChan, p.ctor()))
	}

	// Asynchronously stop all workers > N
	for i := n; i < lWorkers; i++ {
		p.workers[i].stop()
	}

	// Synchronously wait for all workers > N to stop
	for i := n; i < lWorkers; i++ {
		p.workers[i].join()
		p.workers[i] = nil
	}

	// Remove stopped workers from slice
	p.workers = p.workers[:n]
}

// GetSize returns the current size of the pool.
func (p *pool) getSize() int {
	p.workerMut.Lock()
	defer p.workerMut.Unlock()

	return len(p.workers)
}

// Close will terminate all workers and close the job channel of this Pool.
func (p *pool) close() {
	p.setSize(0)
	close(p.reqChan)
}
