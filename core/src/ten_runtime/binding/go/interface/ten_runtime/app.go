//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

package ten

// #include "app.h"
import "C"

import (
	"fmt"
	"runtime"
	"unsafe"
)

// IApp is the interface for the app.
type IApp interface {
	OnConfigure(tenEnv TenEnv)
	OnInit(tenEnv TenEnv)
	OnDeinit(tenEnv TenEnv)
}

// DefaultApp is the default app.
type DefaultApp struct{}

// OnConfigure configures the app.
func (p *DefaultApp) OnConfigure(
	tenEnv TenEnv,
) {
	tenEnv.LogDebug("OnConfigure.")

	tenEnv.OnConfigureDone()
}

// OnInit initializes the app.
func (p *DefaultApp) OnInit(
	tenEnv TenEnv,
) {
	tenEnv.LogDebug("OnInit.")

	tenEnv.OnInitDone()
}

// OnDeinit deinitializes the app.
func (p *DefaultApp) OnDeinit(tenEnv TenEnv) {
	tenEnv.LogDebug("OnDeinit.")

	tenEnv.OnDeinitDone()
}

type app struct {
	IApp
	baseTenObject[*C.ten_go_app_t]
}

var _ App = new(app)

// App is the interface for the app.
type App interface {
	Run(runInBackground bool)
	Close()
	Wait()
}

func (p *app) Run(runInBackground bool) {
	C.ten_go_app_run(p.cPtr, C.bool(runInBackground))
}

func (p *app) Close() {
	C.ten_go_app_close(p.cPtr)
}

func (p *app) Wait() {
	C.ten_go_app_wait(p.cPtr)
}

// NewApp creates a new app.
func NewApp(
	iApp IApp,
) (App, error) {
	if iApp == nil {
		return nil, newTenError(
			ErrorCodeInvalidArgument,
			"iApp is required.",
		)
	}

	pApp := &app{
		IApp: iApp,
	}
	appObjID := newhandle(pApp)
	pApp.goObjID = appObjID
	pApp.cPtr = (*C.ten_go_app_t)(
		unsafe.Pointer(C.ten_go_app_create(C.uintptr_t(appObjID))),
	)

	// C app is created from Go and also destroyed from Go, which means the C
	// app won't be destroyed until the Go finalizer function is called.
	runtime.SetFinalizer(pApp, func(p *app) {
		C.ten_go_app_finalize(p.cPtr)
	})

	return pApp, nil
}

//export tenGoAppOnConfigure
func tenGoAppOnConfigure(
	appID C.uintptr_t,
	tenEnvID C.uintptr_t,
) {
	appObj, ok := handle(appID).get().(*app)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get app from handle map, id: %d.",
				uintptr(appID),
			),
		)
	}

	tenEnvObj, ok := handle(tenEnvID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvID),
			),
		)
	}

	tenEnvInstance, ok := tenEnvObj.(*tenEnv)
	if !ok {
		// Should not happen.
		panic("Invalid ten object type.")
	}

	tenEnvInstance.attachToApp()

	appObj.OnConfigure(tenEnvObj)
}

//export tenGoAppOnInit
func tenGoAppOnInit(
	appID C.uintptr_t,
	tenEnvID C.uintptr_t,
) {
	appObj, ok := handle(appID).get().(*app)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get app from handle map, id: %d.",
				uintptr(appID),
			),
		)
	}

	tenEnvObj, ok := handle(tenEnvID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvID),
			),
		)
	}

	appObj.OnInit(tenEnvObj)
}

//export tenGoAppOnDeinit
func tenGoAppOnDeinit(appID C.uintptr_t, tenEnvID C.uintptr_t) {
	appObj, ok := handle(appID).free().(*app)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get app from handle map, id: %d.",
				uintptr(appID),
			),
		)
	}

	tenEnvObj, ok := handle(tenEnvID).get().(TenEnv)
	if !ok {
		panic(
			fmt.Sprintf(
				"Failed to get ten env from handle map, id: %d.",
				uintptr(tenEnvID),
			),
		)
	}

	appObj.OnDeinit(tenEnvObj)
}
